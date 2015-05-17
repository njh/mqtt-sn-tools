/*
  Basic MQTT-SN client library
  Copyright (C) 2013 Nicholas Humfrey

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
#include <netdb.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "mqtt-sn.h"


#ifndef AI_DEFAULT
#define AI_DEFAULT (AI_ADDRCONFIG|AI_V4MAPPED)
#endif

static uint8_t debug = FALSE;
static uint16_t next_message_id = 1;
static time_t last_transmit = 0;
static time_t last_receive = 0;
static time_t keep_alive = 0;

topic_map_t *topic_map = NULL;


void mqtt_sn_set_debug(uint8_t value)
{
    debug = value;
}

int mqtt_sn_create_socket(const char* host, const char* port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    struct timeval tv;
    int fd, ret;

    // Set options for the resolver
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_DEFAULT;    /* Default flags */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    // Lookup address
    ret = getaddrinfo(host, port, &hints, &result);
    if (ret != 0) {
    	__CUR_TIME__
        fprintf(stderr, "%s getaddrinfo: %s\n", tm_buffer , gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket and)
       try the next address. */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1)
            continue;

        // Connect socket to the remote host
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;      // Success

        close(fd);
    }

    if (rp == NULL) {
    	__CUR_TIME__
        fprintf(stderr, "%s Could not connect to remote host.\n" , tm_buffer);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);

    // FIXME: set the Don't Fragment flag

    // Setup timeout on the socket
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error setting timeout on socket");
    }

    return fd;
}

void mqtt_sn_send_packet(int sock, const void* data)
{
    size_t sent, len = ((uint8_t*)data)[0];

    sent = send(sock, data, len, 0);
    if (sent != len) {
    	__CUR_TIME__
        fprintf(stderr, "%s Warning: only sent %d of %d bytes\n", tm_buffer , (int)sent , (int)len);
    }

    // Store the last time that we sent a packet
    last_transmit = time(NULL);
}

uint8_t mqtt_sn_validate_packet(const void *packet, size_t length)
{
    const uint8_t* buf = packet;

    if (buf[0] == 0x00) {
    	__CUR_TIME__
        fprintf(stderr, "%s Error: packet length header is not valid\n" , tm_buffer);
        return FALSE;
    }

    if (buf[0] == 0x01) {
    	__CUR_TIME__
        fprintf(stderr, "%s Error: packet received is longer than this tool can handle\n" , tm_buffer);
        return FALSE;
    }

    if (buf[0] != length) {
    	__CUR_TIME__
        fprintf(stderr, "%s Error: read %d bytes but packet length is %d bytes.\n", tm_buffer , (int)length, (int)buf[0]);
        return FALSE;
    }

    return TRUE;
}

void* mqtt_sn_receive_packet(int sock)
{
    static uint8_t buffer[MQTT_SN_MAX_PACKET_LENGTH+1];
    int bytes_read;

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s Waiting for a packet...\n" , tm_buffer);
    }

    // Read in the packet
    bytes_read = recv(sock, buffer, MQTT_SN_MAX_PACKET_LENGTH, 0);
    if (bytes_read < 0) {
        if (errno == EAGAIN) {
            if (debug) {
            	__CUR_TIME__
                fprintf(stderr, "%s Timed out waiting for packet.\n" , tm_buffer);
            }
            return NULL;
        } else {
            perror("recv failed");
            exit(EXIT_FAILURE);
        }
    }

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s Received %d bytes. Type=%s.\n" , tm_buffer , (int)bytes_read, mqtt_sn_type_string(buffer[1]));
    }

    if (mqtt_sn_validate_packet(buffer, bytes_read) == FALSE) {
        return NULL;
    }

    // NULL-terminate the packet
    buffer[bytes_read] = '\0';

    // Store the last time that we received a packet
    last_receive = time(NULL);

    return buffer;
}

void mqtt_sn_send_connect(int sock, const char* client_id, uint16_t keepalive)
{
    connect_packet_t packet;

    // Check that it isn't too long
    if (client_id && strlen(client_id) > 23) {
    	__CUR_TIME__
        fprintf(stderr, "%s Error: client id %s is too long\n" , tm_buffer , client_id );
        exit(EXIT_FAILURE);
    }

    // Create the CONNECT packet
    packet.type = MQTT_SN_TYPE_CONNECT;
    packet.flags = MQTT_SN_FLAG_CLEAN;
    packet.protocol_id = MQTT_SN_PROTOCOL_ID;
    packet.duration = htons(keepalive);

    // Generate a Client ID if none given
    if (client_id == NULL || client_id[0] == '\0') {
        snprintf(packet.client_id, sizeof(packet.client_id)-1, "mqtt-sn-tools-%d", getpid());
        packet.client_id[sizeof(packet.client_id) - 1] = '\0';
    } else {
        strncpy(packet.client_id, client_id, sizeof(packet.client_id)-1);
        packet.client_id[sizeof(packet.client_id) - 1] = '\0';
    }

    packet.length = 0x06 + strlen(packet.client_id);

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s %s sending CONNECT packet...\n" , tm_buffer , packet.client_id);
    }

    // Store the keep alive period
    if (keepalive) {
        keep_alive = keepalive;
    }

    return mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_register(int sock, const char* topic_name)
{
    register_packet_t packet;
    size_t topic_name_len = strlen(topic_name);

    if (topic_name_len > MQTT_SN_MAX_TOPIC_LENGTH) {
    	__CUR_TIME__
        fprintf(stderr, "%s Error: topic name %s is too long\n" , tm_buffer , topic_name);
        exit(EXIT_FAILURE);
    }

    packet.type = MQTT_SN_TYPE_REGISTER;
    packet.topic_id = 0;
    packet.message_id = htons(next_message_id++);
    strncpy(packet.topic_name, topic_name, sizeof(packet.topic_name));
    packet.length = 0x06 + topic_name_len;

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s Sending REGISTER %s packet\n" , tm_buffer , topic_name );
    }

    return mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_regack(int sock, int topic_id, int mesage_id)
{
    regack_packet_t packet;
    packet.type = MQTT_SN_TYPE_REGACK;
    packet.topic_id = htons(topic_id);
    packet.message_id = htons(mesage_id);
    packet.return_code = 0x00;
    packet.length = 0x07;

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s Sending REGACK topic id %i packet\n" , tm_buffer , topic_id );
    }

    return mqtt_sn_send_packet(sock, &packet);
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

void mqtt_sn_send_publish(int sock, uint16_t topic_id, uint8_t topic_type, const void* data, int8_t qos, uint8_t retain)
{
    publish_packet_t packet;
    size_t data_len = strlen(data);

    if (data_len > sizeof(packet.data)) {
    	__CUR_TIME__
        fprintf(stderr, "%s Error: payload is too big: %lu\n" , tm_buffer , (long unsigned int) data_len);
        exit(EXIT_FAILURE);
    }

    packet.type = MQTT_SN_TYPE_PUBLISH;
    packet.flags = 0x00;
    if (retain)
        packet.flags += MQTT_SN_FLAG_RETAIN;
    packet.flags += mqtt_sn_get_qos_flag(qos);
    packet.flags += (topic_type & 0x3);
    packet.topic_id = htons(topic_id);
    packet.message_id = htons(next_message_id++);
    strncpy(packet.data, data, sizeof(packet.data));
    packet.length = 0x07 + data_len;

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s Sending PUBLISH packet...\n" , tm_buffer ) ;
    }

    return mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_subscribe_topic_name(int sock, const char* topic_name, uint8_t qos)
{
    subscribe_packet_t packet;
    size_t topic_name_len = strlen(topic_name);

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

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s Sending SUBSCRIBE packet...\n" , tm_buffer );
    }

    return mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_subscribe_topic_id(int sock, uint16_t topic_id, uint8_t qos)
{
    subscribe_packet_t packet;
    packet.type = MQTT_SN_TYPE_SUBSCRIBE;
    packet.flags = 0x00;
    packet.flags += mqtt_sn_get_qos_flag(qos);
    packet.flags += MQTT_SN_TOPIC_TYPE_PREDEFINED;
    packet.message_id = htons(next_message_id++);
    packet.topic_id = htons(topic_id);
    packet.length = 0x05 + 2;

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s Sending SUBSCRIBE packet...\n", tm_buffer );
    }

    return mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_pingreq(int sock)
{
    char packet[2];

    packet[0] = 2;
    packet[1] = MQTT_SN_TYPE_PINGREQ;

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s Sending ping...\n" , tm_buffer );
    }

    return mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_send_disconnect(int sock)
{
    disconnect_packet_t packet;
    packet.type = MQTT_SN_TYPE_DISCONNECT;
    packet.length = 0x02;

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s Sending DISCONNECT packet...\n" , tm_buffer);
    }

    return mqtt_sn_send_packet(sock, &packet);
}

void mqtt_sn_receive_connack(int sock)
{
    connack_packet_t *packet = mqtt_sn_receive_packet(sock);

    if (packet == NULL) {
    	__CUR_TIME__
        fprintf(stderr, "%s Failed to connect to MQTT-SN gateway.\n" , tm_buffer );
        exit(EXIT_FAILURE);
    }

    if (packet->type != MQTT_SN_TYPE_CONNACK) {
    	__CUR_TIME__
        fprintf(stderr, "%s Was expecting CONNACK packet but received: 0x%2.2x\n", tm_buffer , packet->type);
        exit(EXIT_FAILURE);
    }

    // Check Connack return code
    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s CONNACK return code: 0x%2.2x\n", tm_buffer , packet->return_code);
    }

    if (packet->return_code) {
    	__CUR_TIME__
    	fprintf(stderr, "%s CONNECT error: %s\n", tm_buffer , mqtt_sn_return_code_string(packet->return_code));
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
    	__CUR_TIME__
        fprintf(stderr, "%s Error: attempted to register invalid topic id: 0x%4.4x\n", tm_buffer , topic_id);
        return;
    }

    // Check topic name is valid
    if (topic_name == NULL || strlen(topic_name) <= 0) {
    	__CUR_TIME__
        fprintf(stderr, "%s Error: attempted to register invalid topic name.\n" , tm_buffer);
        return;
    }

    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s Registering topic 0x%4.4x: %s\n", tm_buffer , topic_id, topic_name);
    }

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
        	__CUR_TIME__
            fprintf(stderr, "%s Error: Failed to allocate memory for new topic map entry.\n" , tm_buffer);
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

    __CUR_TIME__
    fprintf(stderr, "%s Warning: failed to lookup topic id: 0x%4.4x\n", tm_buffer , topic_id);
    return NULL;
}

uint16_t mqtt_sn_receive_regack(int sock)
{
    regack_packet_t *packet = mqtt_sn_receive_packet(sock);
    uint16_t received_message_id, received_topic_id;

    if (packet == NULL) {
    	__CUR_TIME__
        fprintf(stderr, "%s Failed to connect to register topic.\n" , tm_buffer);
        exit(EXIT_FAILURE);
    }

    if (packet->type != MQTT_SN_TYPE_REGACK) {
    	__CUR_TIME__
        fprintf(stderr, "%s Was expecting REGACK packet but received: 0x%2.2x\n", tm_buffer , packet->type);
        exit(-1);
    }

    // Check Regack return code
    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s REGACK return code: 0x%2.2x\n", tm_buffer , packet->return_code);
    }

    if (packet->return_code) {
    	__CUR_TIME__
        fprintf(stderr, "%s REGISTER error: %s\n", tm_buffer , mqtt_sn_return_code_string(packet->return_code));
        exit(packet->return_code);
    }

    // Check that the Message ID matches
    received_message_id = ntohs( packet->message_id );
    if (received_message_id != next_message_id-1) {
    	__CUR_TIME__
        fprintf(stderr, "%s Warning: message id in Regack does not equal message id sent\n" , tm_buffer);
    }

    // Return the topic ID returned by the gateway
    received_topic_id = ntohs( packet->topic_id );
    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s REGACK topic id: 0x%4.4x\n", tm_buffer , received_topic_id);
    }

    return received_topic_id;
}

uint16_t mqtt_sn_receive_suback(int sock)
{
    suback_packet_t *packet = mqtt_sn_receive_packet(sock);
    uint16_t received_message_id, received_topic_id;

    if (packet == NULL) {
    	__CUR_TIME__
        fprintf(stderr, "%s Failed to subscribe to topic.\n" , tm_buffer );
        exit(EXIT_FAILURE);
    }

    if (packet->type != MQTT_SN_TYPE_SUBACK) {
    	__CUR_TIME__
        fprintf(stderr, "%s Was expecting SUBACK packet but received: 0x%2.2x\n", tm_buffer , packet->type);
        exit(-1);
    }

    // Check Suback return code
    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s SUBACK return code: 0x%2.2x\n", tm_buffer , packet->return_code);
    }

    if (packet->return_code) {
    	__CUR_TIME__
        fprintf(stderr, "%s SUBSCRIBE error: %s\n", tm_buffer , mqtt_sn_return_code_string(packet->return_code));
        exit(packet->return_code);
    }

    // Check that the Message ID matches
    received_message_id = ntohs( packet->message_id );
    if (received_message_id != next_message_id-1) {
    	__CUR_TIME__
        fprintf(stderr, "%s Warning: message id in SUBACK does not equal message id sent\n" , tm_buffer );
        if (debug) {
            fprintf(stderr, " \tExpecting: %d\n", next_message_id-1);
            fprintf(stderr, " \tActual: %d\n", received_message_id);
        }
    }

    // Return the topic ID returned by the gateway
    received_topic_id = ntohs( packet->topic_id );
    if (debug) {
    	__CUR_TIME__
        fprintf(stderr, "%s SUBACK topic id: 0x%4.4x\n", tm_buffer , received_topic_id);
    }

    return received_topic_id;
}

publish_packet_t* mqtt_sn_loop(int sock, int timeout)
{
    time_t now = time(NULL);
    struct timeval tv;
    fd_set rfd;
    int ret;

    // Time to send a ping?
    if (keep_alive > 0 && (now - last_transmit) >= keep_alive) {
        mqtt_sn_send_pingreq(sock);
    }

    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    ret = select(FD_SETSIZE, &rfd, NULL, NULL, &tv);
    if (ret < 0) {
        if (errno != EINTR) {
            // Something is wrong.
            perror("select");
            exit(EXIT_FAILURE);
        }
    } else if (ret > 0) {
        char* packet;

        // Receive a packet
        packet = mqtt_sn_receive_packet(sock);
        if (packet) {
            switch(packet[1]) {
                case MQTT_SN_TYPE_PUBLISH: {
                    return (publish_packet_t*)packet;
                    break;
                }

                case MQTT_SN_TYPE_REGISTER: {
                    mqtt_sn_process_register(sock, (register_packet_t*)packet);
                    break;
                };

                case MQTT_SN_TYPE_PINGRESP: {
                    // do nothing
                    break;
                };

                case MQTT_SN_TYPE_DISCONNECT: {
                	__CUR_TIME__
                    fprintf(stderr, "%s Warning: Received DISCONNECT from gateway.\n" , tm_buffer );
                    exit(EXIT_FAILURE);
                    break;
                };

                default: {
                    const char* type = mqtt_sn_type_string(packet[1]);
                	__CUR_TIME__
                    fprintf(stderr, "%s Warning: unexpected packet type: %s.\n", tm_buffer , type);
                    break;
                }
            }
        }
    }

    // Check for receive timeout
    if (keep_alive > 0 && (now - last_receive) >= (keep_alive * 1.5)) {
    	__CUR_TIME__
        fprintf(stderr, "%s Keep alive error: timed out receiving packet from gateway.\n" , tm_buffer );
        exit(EXIT_FAILURE);
    }

    return NULL;
}

const char* mqtt_sn_type_string(uint8_t type)
{
    switch(type) {
        case MQTT_SN_TYPE_ADVERTISE:     return "ADVERTISE";
        case MQTT_SN_TYPE_SEARCHGW:      return "SEARCHGW";
        case MQTT_SN_TYPE_GWINFO:        return "GWINFO";
        case MQTT_SN_TYPE_CONNECT:       return "CONNECT";
        case MQTT_SN_TYPE_CONNACK:       return "CONNACK";
        case MQTT_SN_TYPE_WILLTOPICREQ:  return "WILLTOPICREQ";
        case MQTT_SN_TYPE_WILLTOPIC:     return "WILLTOPIC";
        case MQTT_SN_TYPE_WILLMSGREQ:    return "WILLMSGREQ";
        case MQTT_SN_TYPE_WILLMSG:       return "WILLMSG";
        case MQTT_SN_TYPE_REGISTER:      return "REGISTER";
        case MQTT_SN_TYPE_REGACK:        return "REGACK";
        case MQTT_SN_TYPE_PUBLISH:       return "PUBLISH";
        case MQTT_SN_TYPE_PUBACK:        return "PUBACK";
        case MQTT_SN_TYPE_PUBCOMP:       return "PUBCOMP";
        case MQTT_SN_TYPE_PUBREC:        return "PUBREC";
        case MQTT_SN_TYPE_PUBREL:        return "PUBREL";
        case MQTT_SN_TYPE_SUBSCRIBE:     return "SUBSCRIBE";
        case MQTT_SN_TYPE_SUBACK:        return "SUBACK";
        case MQTT_SN_TYPE_UNSUBSCRIBE:   return "UNSUBSCRIBE";
        case MQTT_SN_TYPE_UNSUBACK:      return "UNSUBACK";
        case MQTT_SN_TYPE_PINGREQ:       return "PINGREQ";
        case MQTT_SN_TYPE_PINGRESP:      return "PINGRESP";
        case MQTT_SN_TYPE_DISCONNECT:    return "DISCONNECT";
        case MQTT_SN_TYPE_WILLTOPICUPD:  return "WILLTOPICUPD";
        case MQTT_SN_TYPE_WILLTOPICRESP: return "WILLTOPICRESP";
        case MQTT_SN_TYPE_WILLMSGUPD:    return "WILLMSGUPD";
        case MQTT_SN_TYPE_WILLMSGRESP:   return "WILLMSGRESP";
        default:                         return "UNKNOWN";
    }
}

const char* mqtt_sn_return_code_string(uint8_t return_code)
{
    switch(return_code) {
        case 0x00: return "Accepted";
        case 0x01: return "Rejected: congestion";
        case 0x02: return "Rejected: invalid topic ID";
        case 0x03: return "Rejected: not supported";
        default:   return "Rejected: unknown reason";
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
    topic_map = NULL ;
}

