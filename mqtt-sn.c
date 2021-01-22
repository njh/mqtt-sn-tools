/*
  Common functions used by the MQTT-SN Tools
  Copyright (C) Nicholas Humfrey

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

#include "mqtt-sn.h"


#ifndef AI_DEFAULT
#define AI_DEFAULT (AI_ADDRCONFIG|AI_V4MAPPED)
#endif

static uint8_t debug = 0;
static uint8_t verbose = 0;
static uint8_t timeout = MQTT_SN_DEFAULT_TIMEOUT;
static uint16_t next_message_id = 1;
static time_t last_transmit = 0;
static time_t last_receive = 0;
static time_t keep_alive = 0;
static uint8_t forwarder_encapsulation = FALSE;
const uint8_t *wireless_node_id = NULL;
uint8_t wireless_node_id_len  = 0;

topic_map_t *topic_map = NULL;


void mqtt_sn_set_debug(uint8_t value)
{
    debug = value;
    mqtt_sn_log_debug("Debug level is: %d.", debug);
}

void mqtt_sn_set_verbose(uint8_t value)
{
    verbose = value;
    mqtt_sn_log_debug("Verbose level is: %d.", verbose);
}

void mqtt_sn_set_timeout(uint8_t value)
{
    if (value < 1) {
        timeout = MQTT_SN_DEFAULT_TIMEOUT;
    } else {
        timeout = value;
    }
    mqtt_sn_log_debug("Network timeout is: %d seconds.", timeout);
}

int mqtt_sn_create_socket(const char* host, const char* port, uint16_t source_port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    struct timeval tv;
    int fd, ret;

    // Set options for the resolver
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    // Lookup address
    ret = getaddrinfo(host, port, &hints, &result);
    if (ret != 0) {
        mqtt_sn_log_err("getaddrinfo: %s", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket and)
       try the next address. */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        char hoststr[NI_MAXHOST] = "";
        int error = 0;

        // Display the IP address in debug mode
        error = getnameinfo(rp->ai_addr, rp->ai_addrlen,
                            hoststr, sizeof(hoststr), NULL, 0,
                            NI_NUMERICHOST | NI_NUMERICSERV);
        if (error == 0) {
            mqtt_sn_log_debug("Trying %s...", hoststr);
        } else {
            mqtt_sn_log_debug("getnameinfo: %s", gai_strerror(ret));
        }

        // Create a socket
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            mqtt_sn_log_debug("Failed to create socket: %s", strerror(errno));
            continue;
        }

        if (source_port != 0) {
            // Bind socket to the correct port
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = rp->ai_family;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port = htons(source_port);
            if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                mqtt_sn_log_debug("Failed to bind socket: %s", strerror(errno));
                continue;
            }
        }

        // Connect socket to the remote host
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            // Success
            break;
        } else {
            mqtt_sn_log_debug("Connect failed: %s", strerror(errno));
        }

        close(fd);
    }

    if (rp == NULL) {
        mqtt_sn_log_err("Could not connect to remote host.");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);

    // FIXME: set the Don't Fragment flag

    // Setup timeout on the socket
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting timeout on socket");
    }

    return fd;
}

void mqtt_sn_send_packet(int sock, const void* data)
{
    ssize_t sent = 0;
    size_t len = ((uint8_t*)data)[0];

    // If forwarder encapsulation enabled, wrap packet
    if (forwarder_encapsulation) {
        return mqtt_sn_send_frwdencap_packet(sock, data, wireless_node_id, wireless_node_id_len);
    }

    if (debug > 1) {
        mqtt_sn_log_debug("Sending  %2lu bytes. Type=%s on Socket: %d.", (long unsigned int)len,
                          mqtt_sn_type_string(((uint8_t*)data)[1]), sock);
    }

    sent = send(sock, data, len, 0);
    if (sent != len) {
        mqtt_sn_log_warn("Only sent %d of %d bytes", (int)sent, (int)len);
    }

    // Store the last time that we sent a packet
    last_transmit = time(NULL);
}

void mqtt_sn_send_frwdencap_packet(int sock, const void* data, const uint8_t *wireless_node_id, uint8_t wireless_node_id_len)
{
    ssize_t sent = 0;
    size_t len = ((uint8_t*)data)[0];
    uint8_t orig_packet_type = ((uint8_t*)data)[1];
    frwdencap_packet_t *packet;

    packet = mqtt_sn_create_frwdencap_packet(data, &len, wireless_node_id, wireless_node_id_len);

    if (debug > 1) {
        mqtt_sn_log_debug("Sending  %2lu bytes. Type=%s with %s inside on Socket: %d.", (long unsigned int)len,
                          mqtt_sn_type_string(packet->type), mqtt_sn_type_string(orig_packet_type), sock);
    }

    sent = send(sock, packet, len, 0);
    if (sent != len) {
        mqtt_sn_log_debug("Warning: only sent %d of %d bytes.", (int)sent, (int)len);
    }

    // Store the last time that we sent a packet
    last_transmit = time(NULL);

    free(packet);
}

uint8_t mqtt_sn_validate_packet(const void *packet, size_t length)
{
    const uint8_t* buf = packet;

    if (buf[0] == 0x00) {
        mqtt_sn_log_warn("Packet length header is not valid");
        return FALSE;
    }

    if (buf[0] == 0x01) {
        mqtt_sn_log_warn("Packet received is longer than this tool can handle");
        return FALSE;
    }

    // When forwarder encapsulation is enabled each packet must be FRWDENCAP type
    if (forwarder_encapsulation && buf[1] != MQTT_SN_TYPE_FRWDENCAP) {
        mqtt_sn_log_warn("Expecting FRWDENCAP packet and got Type=%s.", mqtt_sn_type_string(buf[1]));
        return FALSE;
    }

    // If packet is forwarder encapsulation expected packet length is sum of forwarder encapsulation
    // header and length of encapsulated packet.
    if ((buf[1] == MQTT_SN_TYPE_FRWDENCAP && buf[0] + buf[buf[0]] != length) ||
            (buf[1] != MQTT_SN_TYPE_FRWDENCAP && buf[0] != length)) {
        mqtt_sn_log_warn("Read %d bytes but packet length is %d bytes.", (int)length,
                         buf[1] != MQTT_SN_TYPE_FRWDENCAP ? (int)buf[0] : (int)(buf[0] + buf[buf[0]]));
        return FALSE;
    }

    return TRUE;
}

void* mqtt_sn_receive_packet(int sock)
{
    uint8_t *wireless_node_id  = NULL;
    uint8_t wireless_node_id_len = 0;

    return mqtt_sn_receive_frwdencap_packet(sock, &wireless_node_id, &wireless_node_id_len);
}

void* mqtt_sn_receive_frwdencap_packet(int sock, uint8_t **wireless_node_id, uint8_t *wireless_node_id_len)
{
    static uint8_t buffer[MQTT_SN_MAX_PACKET_LENGTH + MQTT_SN_MAX_WIRELESS_NODE_ID_LENGTH + 3  + 1];
    struct sockaddr_storage addr;
    socklen_t slen = sizeof(addr);
    uint8_t *packet = buffer;
    ssize_t bytes_read;

    *wireless_node_id = NULL;
    *wireless_node_id_len = 0;

    mqtt_sn_log_debug("waiting for packet...");

    // Read in the packet
    bytes_read = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &slen);
    if (bytes_read < 0) {
        if (errno == EAGAIN) {
            mqtt_sn_log_debug("Timed out waiting for packet.");
            return NULL;
        } else {
            perror("recv failed");
            exit(EXIT_FAILURE);
        }
    }

    // Convert the source address into a string
    if (debug) {
        char addrstr[INET6_ADDRSTRLEN] = "unknown";
        uint16_t port = 0;

        if (addr.ss_family == AF_INET) {
            struct sockaddr_in *in = (struct sockaddr_in *)&addr;
            inet_ntop(AF_INET, &in->sin_addr, addrstr, sizeof(struct sockaddr_in));
            port = ntohs(in->sin_port);
        } else if (addr.ss_family == AF_INET6) {
            struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&addr;
            inet_ntop(AF_INET6, &in6->sin6_addr, addrstr, sizeof(struct sockaddr_in6));
            port = ntohs(in6->sin6_port);
        }

        if (packet[1] == MQTT_SN_TYPE_FRWDENCAP) {
            mqtt_sn_log_debug("Received %2d bytes from %s:%d. Type=%s with %s inside on Socket: %d",
                              (int)bytes_read, addrstr, port,
                              mqtt_sn_type_string(buffer[1]), mqtt_sn_type_string(packet[packet[0] + 1]), sock);
        } else {
            mqtt_sn_log_debug("Received %2d bytes from %s:%d. Type=%s on Socket: %d",
                              (int)bytes_read, addrstr, port,
                              mqtt_sn_type_string(buffer[1]), sock);
        }
    }

    if (mqtt_sn_validate_packet(buffer, bytes_read) == FALSE) {
        return NULL;
    }

    // NULL-terminate the packet
    buffer[bytes_read] = '\0';

    if (packet[1] == MQTT_SN_TYPE_FRWDENCAP) {
        *wireless_node_id = &packet[3];
        *wireless_node_id_len = packet[0] - 3;
        // Shift packet by the actual length of FRWDENCAP header
        packet += packet[0];
    }

    // Store the last time that we received a packet
    last_receive = time(NULL);

    return packet;
}

void mqtt_sn_send_connect(int sock, const char* client_id, uint16_t keepalive, uint8_t clean_session)
{
    connect_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    // Check that it isn't too long
    if (client_id && strlen(client_id) > MQTT_SN_MAX_CLIENT_ID_LENGTH) {
        mqtt_sn_log_err("Client id is too long");
        exit(EXIT_FAILURE);
    }

    // Create the CONNECT packet
    packet.type = MQTT_SN_TYPE_CONNECT;
    packet.flags = clean_session ? MQTT_SN_FLAG_CLEAN : 0;
    packet.protocol_id = MQTT_SN_PROTOCOL_ID;
    packet.duration = htons(keepalive);

    // Generate a Client ID if none given
    if (client_id == NULL || client_id[0] == '\0') {
        snprintf(packet.client_id, MQTT_SN_MAX_CLIENT_ID_LENGTH, "mqtt-sn-tools-%d", getpid());
    } else {
        memcpy(packet.client_id, client_id, strlen(client_id));
    }

    packet.length = 0x06 + strlen(packet.client_id);

    mqtt_sn_log_debug("Sending CONNECT packet...");

    // Store the keep alive period
    if (keepalive) {
        keep_alive = keepalive;
    }

    mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_register(int sock, const char* topic_name)
{
    size_t topic_name_len = strlen(topic_name);
    register_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (topic_name_len > MQTT_SN_MAX_TOPIC_LENGTH) {
        mqtt_sn_log_err("Topic name is too long");
        exit(EXIT_FAILURE);
    }

    packet.type = MQTT_SN_TYPE_REGISTER;
    packet.topic_id = 0;
    packet.message_id = htons(next_message_id++);
    strncpy(packet.topic_name, topic_name, sizeof(packet.topic_name));
    packet.length = 0x06 + topic_name_len;

    mqtt_sn_log_debug("Sending REGISTER packet...");

    mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_regack(int sock, int topic_id, int mesage_id)
{
    regack_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    packet.type = MQTT_SN_TYPE_REGACK;
    packet.topic_id = htons(topic_id);
    packet.message_id = htons(mesage_id);
    packet.return_code = 0x00;
    packet.length = 0x07;

    mqtt_sn_log_debug("Sending REGACK packet...");

    mqtt_sn_send_packet(sock, &packet);
}

static uint8_t mqtt_sn_get_qos_flag(int8_t qos)
{
    switch (qos) {
        case -1:
            return MQTT_SN_FLAG_QOS_N1;
        case 0:
            return MQTT_SN_FLAG_QOS_0;
        case 1:
            return MQTT_SN_FLAG_QOS_1;
        case 2:
            return MQTT_SN_FLAG_QOS_2;
        default:
            return 0;
    }
}

void mqtt_sn_send_publish(int sock, uint16_t topic_id, uint8_t topic_type, const void* data, uint16_t data_len, int8_t qos, uint8_t retain)
{
    publish_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (data_len > sizeof(packet.data)) {
        mqtt_sn_log_err("Payload is too big");
        exit(EXIT_FAILURE);
    }

    packet.type = MQTT_SN_TYPE_PUBLISH;
    packet.flags = 0x00;
    if (retain)
        packet.flags += MQTT_SN_FLAG_RETAIN;
    packet.flags += mqtt_sn_get_qos_flag(qos);
    packet.flags += (topic_type & 0x3);
    packet.topic_id = htons(topic_id);
    if (qos > 0) {
        packet.message_id = htons(next_message_id++);
    } else {
        packet.message_id = 0x0000;
    }
    memcpy(packet.data, data, sizeof(packet.data));
    packet.length = 0x07 + data_len;

    mqtt_sn_log_debug("Sending PUBLISH packet...");
    mqtt_sn_send_packet(sock, &packet);

    if (qos == 1) {
        // Now wait for a PUBACK
        puback_packet_t *packet = mqtt_sn_wait_for(MQTT_SN_TYPE_PUBACK, sock);
        if (packet) {
            mqtt_sn_log_debug("Received PUBACK");
        } else {
            mqtt_sn_log_warn("Failed to receive PUBACK after PUBLISH");
        }
    }
}

void mqtt_sn_send_puback(int sock, publish_packet_t* publish, uint8_t return_code)
{
    puback_packet_t puback;
    memset(&puback, 0, sizeof(puback));

    puback.type = MQTT_SN_TYPE_PUBACK;
    puback.topic_id = publish->topic_id;
    puback.message_id = publish->message_id;
    puback.return_code = return_code;
    puback.length = 0x07;

    mqtt_sn_log_debug("Sending PUBACK packet...");

    mqtt_sn_send_packet(sock, &puback);
}

void mqtt_sn_send_subscribe_topic_name(int sock, const char* topic_name, uint8_t qos)
{
    size_t topic_name_len = strlen(topic_name);
    subscribe_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    packet.type = MQTT_SN_TYPE_SUBSCRIBE;
    packet.flags = 0x00;
    packet.flags += mqtt_sn_get_qos_flag(qos);
    if (topic_name_len == 2) {
        packet.flags += MQTT_SN_TOPIC_TYPE_SHORT;
    } else {
        packet.flags += MQTT_SN_TOPIC_TYPE_NORMAL;
    }
    packet.message_id = htons(next_message_id++);
    strncpy(packet.topic_name, topic_name, sizeof(packet.topic_name));
    packet.topic_name[sizeof(packet.topic_name)-1] = '\0';
    packet.length = 0x05 + topic_name_len;

    mqtt_sn_log_debug("Sending SUBSCRIBE packet...");

    mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_subscribe_topic_id(int sock, uint16_t topic_id, uint8_t qos)
{
    subscribe_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    packet.type = MQTT_SN_TYPE_SUBSCRIBE;
    packet.flags = 0x00;
    packet.flags += mqtt_sn_get_qos_flag(qos);
    packet.flags += MQTT_SN_TOPIC_TYPE_PREDEFINED;
    packet.message_id = htons(next_message_id++);
    packet.topic_id = htons(topic_id);
    packet.length = 0x05 + 2;

    mqtt_sn_log_debug("Sending SUBSCRIBE packet...");

    mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_pingreq(int sock)
{
    char packet[2];

    packet[0] = 2;
    packet[1] = MQTT_SN_TYPE_PINGREQ;

    mqtt_sn_log_debug("Sending PINGREQ packet...");

    mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_disconnect(int sock, uint16_t duration)
{
    disconnect_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    packet.type = MQTT_SN_TYPE_DISCONNECT;
    if (duration == 0) {
        packet.length = 0x02;
        mqtt_sn_log_debug("Sending DISCONNECT packet...");
    } else {
        packet.length = sizeof(packet);
        packet.duration = htons(duration);
        mqtt_sn_log_debug("Sending DISCONNECT packet with Duration %d...", duration);
    }

    mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_receive_disconnect(int sock)
{
    disconnect_packet_t *packet = mqtt_sn_wait_for(MQTT_SN_TYPE_DISCONNECT, sock);

    if (packet == NULL) {
        mqtt_sn_log_err("Failed to disconnect from MQTT-SN gateway.");
        exit(EXIT_FAILURE);
    }

    // Check Disconnect return duration
    if (packet->length == 4) {
        mqtt_sn_log_warn("DISCONNECT warning. Gateway returned duration in disconnect packet: 0x%2.2x", packet->duration);
    }
}


void mqtt_sn_receive_connack(int sock)
{
    connack_packet_t *packet = mqtt_sn_receive_packet(sock);

    if (packet == NULL) {
        mqtt_sn_log_err("Failed to connect to MQTT-SN gateway.");
        exit(EXIT_FAILURE);
    }

    if (packet->type != MQTT_SN_TYPE_CONNACK) {
        mqtt_sn_log_err("Was expecting CONNACK packet but received: %s", mqtt_sn_type_string(packet->type));
        exit(EXIT_FAILURE);
    }

    // Check Connack return code
    mqtt_sn_log_debug("CONNACK return code: 0x%2.2x", packet->return_code);

    if (packet->return_code) {
        mqtt_sn_log_err("CONNECT error: %s", mqtt_sn_return_code_string(packet->return_code));
        exit(packet->return_code);
    }
}

static int mqtt_sn_process_register(int sock, const register_packet_t *packet)
{
    int message_id = ntohs(packet->message_id);
    int topic_id = ntohs(packet->topic_id);
    const char* topic_name = packet->topic_name;

    // Add it to the topic map
    mqtt_sn_register_topic(topic_id, topic_name);

    // Respond to gateway with REGACK
    mqtt_sn_send_regack(sock, topic_id, message_id);

    return 0;
}

void mqtt_sn_register_topic(int topic_id, const char* topic_name)
{
    topic_map_t **ptr = &topic_map;

    // Check topic ID is valid
    if (topic_id == 0x0000 || topic_id == 0xFFFF) {
        mqtt_sn_log_err("Attempted to register invalid topic id: 0x%4.4x", topic_id);
        return;
    }

    // Check topic name is valid
    if (topic_name == NULL || strlen(topic_name) <= 0) {
        mqtt_sn_log_err("Attempted to register invalid topic name.");
        return;
    }

    mqtt_sn_log_debug("Registering topic 0x%4.4x: %s", topic_id, topic_name);

    // Look for the topic id
    while (*ptr) {
        if ((*ptr)->topic_id == topic_id) {
            break;
        } else {
            ptr = &((*ptr)->next);
        }
    }

    // Allocate memory for a new entry, if we reached the end of the list
    if (*ptr == NULL) {
        *ptr = (topic_map_t *)malloc(sizeof(topic_map_t));
        if (!*ptr) {
            mqtt_sn_log_err("Failed to allocate memory for new topic map entry.");
            exit(EXIT_FAILURE);
        }
        (*ptr)->next = NULL;
    }

    // Copy in the name to the entry
    strncpy((*ptr)->topic_name, topic_name, MQTT_SN_MAX_TOPIC_LENGTH);
    (*ptr)->topic_id = topic_id;
}

const char* mqtt_sn_lookup_topic(int topic_id)
{
    topic_map_t **ptr = &topic_map;

    while (*ptr) {
        if ((*ptr)->topic_id == topic_id) {
            return (*ptr)->topic_name;
        }
        ptr = &((*ptr)->next);
    }

    mqtt_sn_log_warn("Failed to lookup topic id: 0x%4.4x", topic_id);
    return NULL;
}

uint16_t mqtt_sn_receive_regack(int sock)
{
    regack_packet_t *packet = mqtt_sn_wait_for(MQTT_SN_TYPE_REGACK, sock);
    uint16_t received_message_id, received_topic_id;

    if (packet == NULL) {
        mqtt_sn_log_err("Failed to connect to register topic.");
        exit(EXIT_FAILURE);
    }

    // Check Regack return code
    mqtt_sn_log_debug("REGACK return code: 0x%2.2x", packet->return_code);

    if (packet->return_code) {
        mqtt_sn_log_err("REGISTER failed: %s", mqtt_sn_return_code_string(packet->return_code));
        exit(packet->return_code);
    }

    // Check that the Message ID matches
    received_message_id = ntohs(packet->message_id);
    if (received_message_id != next_message_id-1) {
        mqtt_sn_log_warn("Message id in Regack does not equal message id sent");
    }

    // Return the topic ID returned by the gateway
    received_topic_id = ntohs(packet->topic_id);
    mqtt_sn_log_debug("REGACK topic id: 0x%4.4x", received_topic_id);

    return received_topic_id;
}

void mqtt_sn_dump_packet(char* packet)
{
    printf("%s: len=%d", mqtt_sn_type_string(packet[1]), packet[0]);

    switch(packet[1]) {
        case MQTT_SN_TYPE_CONNECT: {
            connect_packet_t* cpkt = (connect_packet_t*)packet;
            printf(" protocol_id=%d", cpkt->protocol_id);
            printf(" duration=%d", ntohs(cpkt->duration));
            printf(" client_id=%s", cpkt->client_id);
            break;
        }
        case MQTT_SN_TYPE_CONNACK: {
            connack_packet_t* capkt = (connack_packet_t*)packet;
            printf(" return_code=%d (%s)", capkt->return_code, mqtt_sn_return_code_string(capkt->return_code));
            break;
        }
        case MQTT_SN_TYPE_REGISTER: {
            register_packet_t* rpkt = (register_packet_t*)packet;
            printf(" topic_id=0x%4.4x", ntohs(rpkt->topic_id));
            printf(" message_id=0x%4.4x", ntohs(rpkt->message_id));
            printf(" topic_name=%s", rpkt->topic_name);
            break;
        }
        case MQTT_SN_TYPE_REGACK: {
            regack_packet_t* rapkt = (regack_packet_t*)packet;
            printf(" topic_id=0x%4.4x", ntohs(rapkt->topic_id));
            printf(" message_id=0x%4.4x", ntohs(rapkt->message_id));
            printf(" return_code=%d (%s)", rapkt->return_code, mqtt_sn_return_code_string(rapkt->return_code));
            break;
        }
        case MQTT_SN_TYPE_PUBLISH: {
            publish_packet_t* ppkt = (publish_packet_t*)packet;
            printf(" topic_id=0x%4.4x", ntohs(ppkt->topic_id));
            printf(" message_id=0x%4.4x", ntohs(ppkt->message_id));
            printf(" data=%s", ppkt->data);
            break;
        }
        case MQTT_SN_TYPE_SUBSCRIBE: {
            subscribe_packet_t* spkt = (subscribe_packet_t*)packet;
            printf(" message_id=0x%4.4x", ntohs(spkt->message_id));
            break;
        }
        case MQTT_SN_TYPE_SUBACK: {
            suback_packet_t* sapkt = (suback_packet_t*)packet;
            printf(" topic_id=0x%4.4x", ntohs(sapkt->topic_id));
            printf(" message_id=0x%4.4x", ntohs(sapkt->message_id));
            printf(" return_code=%d (%s)", sapkt->return_code, mqtt_sn_return_code_string(sapkt->return_code));
            break;
        }
        case MQTT_SN_TYPE_DISCONNECT: {
            disconnect_packet_t* dpkt = (disconnect_packet_t*)packet;
            printf(" duration=%d", ntohs(dpkt->duration));
            break;
        }
    }

    printf("\n");
}

void mqtt_sn_print_publish_packet(publish_packet_t* packet)
{
    if (verbose) {
        int topic_type = packet->flags & 0x3;
        int topic_id = ntohs(packet->topic_id);
        if (verbose == 2) {
            time_t rcv_time;
            char tm_buffer [40];
            time(&rcv_time) ;
            strftime(tm_buffer, 40, "%F %T ", localtime(&rcv_time));
            fputs(tm_buffer, stdout);
        }
        switch (topic_type) {
            case MQTT_SN_TOPIC_TYPE_NORMAL: {
                const char *topic_name = mqtt_sn_lookup_topic(topic_id);
                if (topic_name) {
                    printf("%s: %s\n", topic_name, packet->data);
                }
                break;
            };
            case MQTT_SN_TOPIC_TYPE_PREDEFINED: {
                printf("%4.4x: %s\n", topic_id, packet->data);
                break;
            };
            case MQTT_SN_TOPIC_TYPE_SHORT: {
                const char *str = (const char*)&packet->topic_id;
                printf("%c%c: %s\n", str[0], str[1], packet->data);
                break;
            };
        }
    } else {
        printf("%s\n", packet->data);
    }
}

uint16_t mqtt_sn_receive_suback(int sock)
{
    suback_packet_t *packet = mqtt_sn_wait_for(MQTT_SN_TYPE_SUBACK, sock);
    uint16_t received_message_id, received_topic_id;

    if (packet == NULL) {
        mqtt_sn_log_err("Failed to subscribe to topic.");
        exit(EXIT_FAILURE);
    }

    // Check Suback return code
    mqtt_sn_log_debug("SUBACK return code: 0x%2.2x", packet->return_code);

    if (packet->return_code) {
        mqtt_sn_log_err("SUBSCRIBE error: %s", mqtt_sn_return_code_string(packet->return_code));
        exit(packet->return_code);
    }

    // Check that the Message ID matches
    received_message_id = ntohs(packet->message_id);
    if (received_message_id != next_message_id-1) {
        mqtt_sn_log_warn("Message id in SUBACK does not equal message id sent");
        mqtt_sn_log_debug("  Expecting: %d", next_message_id-1);
        mqtt_sn_log_debug("  Actual: %d", received_message_id);
    }

    // Return the topic ID returned by the gateway
    received_topic_id = ntohs(packet->topic_id);
    mqtt_sn_log_debug("SUBACK topic id: 0x%4.4x", received_topic_id);

    return received_topic_id;
}

int mqtt_sn_select(int sock)
{
    struct timeval tv;
    fd_set rfd;
    int ret;

    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    ret = select(sock + 1, &rfd, NULL, NULL, &tv);
    if (ret < 0 && errno != EINTR) {
        // Something is wrong.
        perror("select");
        exit(EXIT_FAILURE);
    }

    return ret;
}

void* mqtt_sn_wait_for(uint8_t type, int sock)
{
    time_t started_waiting = time(NULL);

    while(TRUE) {
        time_t now = time(NULL);
        int ret;

        // Time to send a ping?
        if (keep_alive > 0 && (now - last_transmit) >= keep_alive) {
            mqtt_sn_send_pingreq(sock);
        }

        ret = mqtt_sn_select(sock);
        if (ret < 0) {
            break;
        } else if (ret > 0) {
            char* packet = mqtt_sn_receive_packet(sock);
            if (packet) {
                switch(packet[1]) {
                    case MQTT_SN_TYPE_PUBLISH:
                        mqtt_sn_print_publish_packet((publish_packet_t *)packet);
                        break;

                    case MQTT_SN_TYPE_REGISTER:
                        mqtt_sn_process_register(sock, (register_packet_t*)packet);
                        break;

                    case MQTT_SN_TYPE_PINGRESP:
                        // do nothing
                        break;

                    case MQTT_SN_TYPE_DISCONNECT:
                        if (type != MQTT_SN_TYPE_DISCONNECT) {
                            mqtt_sn_log_warn("Received DISCONNECT from gateway.");
                            exit(EXIT_FAILURE);
                        }
                        break;

                    default:
                        if (packet[1] != type) {
                            mqtt_sn_log_warn(
                                "Was expecting %s packet but received: %s",
                                mqtt_sn_type_string(type),
                                mqtt_sn_type_string(packet[1])
                            );
                        }
                        break;
                }

                // Did we find what we were looking for?
                if (packet[1] == type) {
                    return packet;
                }
            }
        }

        // Check for receive timeout
        if (keep_alive > 0 && (now - last_receive) >= (keep_alive * 1.5)) {
            mqtt_sn_log_err("Keep alive error: timed out while waiting for a %s from gateway.", mqtt_sn_type_string(type));
            exit(EXIT_FAILURE);
        }

        // Check if we have timed out waiting for the packet we are looking for
        if ((now - started_waiting) >= timeout) {
            mqtt_sn_log_debug("Timed out while waiting for a %s from gateway.", mqtt_sn_type_string(type));
            break;
        }
    }

    return NULL;
}

const char* mqtt_sn_type_string(uint8_t type)
{
    switch(type) {
        case MQTT_SN_TYPE_ADVERTISE:
            return "ADVERTISE";
        case MQTT_SN_TYPE_SEARCHGW:
            return "SEARCHGW";
        case MQTT_SN_TYPE_GWINFO:
            return "GWINFO";
        case MQTT_SN_TYPE_CONNECT:
            return "CONNECT";
        case MQTT_SN_TYPE_CONNACK:
            return "CONNACK";
        case MQTT_SN_TYPE_WILLTOPICREQ:
            return "WILLTOPICREQ";
        case MQTT_SN_TYPE_WILLTOPIC:
            return "WILLTOPIC";
        case MQTT_SN_TYPE_WILLMSGREQ:
            return "WILLMSGREQ";
        case MQTT_SN_TYPE_WILLMSG:
            return "WILLMSG";
        case MQTT_SN_TYPE_REGISTER:
            return "REGISTER";
        case MQTT_SN_TYPE_REGACK:
            return "REGACK";
        case MQTT_SN_TYPE_PUBLISH:
            return "PUBLISH";
        case MQTT_SN_TYPE_PUBACK:
            return "PUBACK";
        case MQTT_SN_TYPE_PUBCOMP:
            return "PUBCOMP";
        case MQTT_SN_TYPE_PUBREC:
            return "PUBREC";
        case MQTT_SN_TYPE_PUBREL:
            return "PUBREL";
        case MQTT_SN_TYPE_SUBSCRIBE:
            return "SUBSCRIBE";
        case MQTT_SN_TYPE_SUBACK:
            return "SUBACK";
        case MQTT_SN_TYPE_UNSUBSCRIBE:
            return "UNSUBSCRIBE";
        case MQTT_SN_TYPE_UNSUBACK:
            return "UNSUBACK";
        case MQTT_SN_TYPE_PINGREQ:
            return "PINGREQ";
        case MQTT_SN_TYPE_PINGRESP:
            return "PINGRESP";
        case MQTT_SN_TYPE_DISCONNECT:
            return "DISCONNECT";
        case MQTT_SN_TYPE_WILLTOPICUPD:
            return "WILLTOPICUPD";
        case MQTT_SN_TYPE_WILLTOPICRESP:
            return "WILLTOPICRESP";
        case MQTT_SN_TYPE_WILLMSGUPD:
            return "WILLMSGUPD";
        case MQTT_SN_TYPE_WILLMSGRESP:
            return "WILLMSGRESP";
        case MQTT_SN_TYPE_FRWDENCAP:
            return "FRWDENCAP";
        default:
            return "UNKNOWN";
    }
}

const char* mqtt_sn_return_code_string(uint8_t return_code)
{
    switch(return_code) {
        case MQTT_SN_ACCEPTED:
            return "Accepted";
        case MQTT_SN_REJECTED_CONGESTION:
            return "Rejected: congestion";
        case MQTT_SN_REJECTED_INVALID:
            return "Rejected: invalid topic ID";
        case MQTT_SN_REJECTED_NOT_SUPPORTED:
            return "Rejected: not supported";
        default:
            return "Rejected: unknown reason";
    }
}

void mqtt_sn_cleanup()
{
    topic_map_t *ptr = topic_map;
    topic_map_t *ptr2 = NULL;

    // Walk through the topic map, deleting each entry
    while (ptr) {
        ptr2 = ptr;
        ptr = ptr->next;
        free(ptr2);
    }
    topic_map = NULL;
}


uint8_t mqtt_sn_enable_frwdencap()
{
    return forwarder_encapsulation = TRUE;
}


uint8_t mqtt_sn_disable_frwdencap()
{
    return forwarder_encapsulation = FALSE;
}


void mqtt_sn_set_frwdencap_parameters(const uint8_t *wlnid, uint8_t wlnid_len)
{
    wireless_node_id = wlnid;
    wireless_node_id_len = wlnid_len;
}


frwdencap_packet_t* mqtt_sn_create_frwdencap_packet(const void *data, size_t *len, const uint8_t *wireless_node_id, uint8_t wireless_node_id_len)
{
    frwdencap_packet_t* packet = NULL;

    // Check that it isn't too long
    if (wireless_node_id_len > MQTT_SN_MAX_WIRELESS_NODE_ID_LENGTH) {
        mqtt_sn_log_err("Wireless node id is longer than %d", MQTT_SN_MAX_WIRELESS_NODE_ID_LENGTH);
        exit(EXIT_FAILURE);
    }

    // Allocates a block of memory for an array of num elements, each of them size bytes long,
    // and initializes all its bits to zero.
    packet = malloc(sizeof(frwdencap_packet_t));
    packet->type = MQTT_SN_TYPE_FRWDENCAP;
    packet->ctrl = 0;

    // Generate a Wireless Node ID if none given
    if (wireless_node_id == NULL || wireless_node_id_len == 0) {
        // A null character is automatically appended after the content written.
        snprintf((char*)packet->wireless_node_id, sizeof(packet->wireless_node_id)-1, "%X", getpid());
        wireless_node_id_len = strlen((char*)packet->wireless_node_id);
    } else {
        memcpy(packet->wireless_node_id, wireless_node_id, wireless_node_id_len);
    }

    packet->length = wireless_node_id_len + 3;

    // Copy mqtt-sn packet into forwarder encapsulation packet
    memcpy(&(packet->wireless_node_id[wireless_node_id_len]), data, ((uint8_t*)data)[0]);

    // Set new packet length to send
    *len = packet->length + ((uint8_t*)data)[0];

    if (debug > 2) {
        char wlnd[65];
        char* buf_ptr = wlnd;
        int i;
        for (i = 0; i < wireless_node_id_len; i++) {
            buf_ptr += sprintf(buf_ptr, "%02X", packet->wireless_node_id[i]);
        }
        *(++buf_ptr) = '\0';

        mqtt_sn_log_debug("Node id: 0x%s, N. id len: %d, Wrapped packet len: %d, Total len: %lu",
                          wlnd, wireless_node_id_len, ((uint8_t*)data)[0], (long unsigned int)*len);
    }

    return packet;
}


static void mqtt_sn_log_msg(const char* level, const char* format, va_list arglist)
{
    time_t mqtt_sn_log_time;
    char tm_buffer[40];

    time(&mqtt_sn_log_time);
    strftime(tm_buffer, sizeof(tm_buffer), "%F %T ", localtime(&mqtt_sn_log_time));

    fputs(tm_buffer, stderr);
    fputs(level, stderr);
    vfprintf(stderr, format, arglist);
    fputs("\n", stderr);
}

void mqtt_sn_log_debug(const char * format, ...)
{
    if (debug) {
        va_list arglist;
        va_start(arglist, format);
        mqtt_sn_log_msg("DEBUG ", format, arglist);
        va_end(arglist);
    }
}

void mqtt_sn_log_warn(const char * format, ...)
{
    va_list arglist;
    va_start(arglist, format);
    mqtt_sn_log_msg("WARN  ", format, arglist);
    va_end(arglist);
}

void mqtt_sn_log_err(const char * format, ...)
{
    va_list arglist;
    va_start(arglist, format);
    mqtt_sn_log_msg("ERROR ", format, arglist);
    va_end(arglist);
}
