/*
  Basic MQTT-S client library
  Copyright (C) 2013 Nicholas Humfrey

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License (http://www.gnu.org/copyleft/gpl.html)
  for more details.
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

#include "mqtts.h"


#ifndef AI_DEFAULT
#define AI_DEFAULT (AI_ADDRCONFIG|AI_V4MAPPED)
#endif

static uint8_t debug = FALSE;
static uint16_t next_message_id = 1;
static time_t last_transmit = 0;
static time_t last_receive = 0;
static time_t keep_alive = 0;


void mqtts_set_debug(uint8_t value)
{
    debug = value;
}

int mqtts_create_socket(const char* host, const char* port)
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
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
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
        fprintf(stderr, "Could not connect to remote host.\n");
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

static void send_packet(int sock, char* data, size_t len)
{
    size_t sent = send(sock, data, len, 0);
    if (sent != len) {
        fprintf(stderr, "Warning: only sent %d of %d bytes\n", (int)sent, (int)len);
    }

    // Store the last time that we sent a packet
    last_transmit = time(NULL);
}

static void* recieve_packet(int sock)
{
    static uint8_t buffer[MQTTS_MAX_PACKET_LENGTH+1];
    int length;
    int bytes_read;

    if (debug)
        fprintf(stderr, "waiting for packet...\n");

    // Read in the packet
    bytes_read = recv(sock, buffer, MQTTS_MAX_PACKET_LENGTH, 0);
    if (bytes_read < 0) {
        if (errno == EAGAIN) {
            if (debug)
                fprintf(stderr, "Timed out waiting for packet.\n");
            return NULL;
        } else {
            perror("recv failed");
            exit(EXIT_FAILURE);
        }
    }

    if (debug)
        fprintf(stderr, "Received %d bytes. Type=%s.\n", (int)bytes_read, mqtts_type_string(buffer[1]));

    length = buffer[0];
    if (length == 0x01) {
        fprintf(stderr, "Error: packet received is longer than this tool can handle\n");
        exit(EXIT_FAILURE);
    }

    if (length != bytes_read) {
        fprintf(stderr, "Warning: read %d bytes but packet length is %d bytes.\n", (int)bytes_read, length);
    }

    // NULL-terminate the packet
    buffer[length] = '\0';

    // Store the last time that we received a packet
    last_receive = time(NULL);

    return buffer;
}

void mqtts_send_connect(int sock, const char* client_id, uint16_t keepalive)
{
    connect_packet_t packet;

    // Create the CONNECT packet
    packet.type = MQTTS_TYPE_CONNECT;
    packet.flags = MQTTS_FLAG_CLEAN;
    packet.protocol_id = MQTTS_PROTOCOL_ID;
    packet.duration = htons(keepalive);

    // Generate a Client ID if none given
    if (client_id == NULL || client_id[0] == '\0') {
        snprintf(packet.client_id, sizeof(packet.client_id)-1, "mqtts-tools-%d", getpid());
        packet.client_id[sizeof(packet.client_id) - 1] = '\0';
    } else {
        strncpy(packet.client_id, client_id, sizeof(packet.client_id)-1);
        packet.client_id[sizeof(packet.client_id) - 1] = '\0';
    }

    packet.length = 0x06 + strlen(packet.client_id);

    if (debug)
        fprintf(stderr, "Sending CONNECT packet...\n");

    // Store the keep alive period
    if (keepalive) {
        keep_alive = keepalive;
    }

    return send_packet(sock, (char*)&packet, packet.length);
}

void mqtts_send_register(int sock, const char* topic_name)
{
    register_packet_t packet;
    packet.type = MQTTS_TYPE_REGISTER;
    packet.topic_id = 0;
    packet.message_id = htons(next_message_id++);
    strncpy(packet.topic_name, topic_name, sizeof(packet.topic_name));
    packet.length = 0x06 + strlen(packet.topic_name);

    if (debug)
        fprintf(stderr, "Sending REGISTER packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}

void mqtts_send_publish(int sock, uint16_t topic_id, uint8_t topic_type, const char* data, int8_t qos, uint8_t retain)
{
    publish_packet_t packet;
    packet.type = MQTTS_TYPE_PUBLISH;
    packet.flags = 0x00;
    if (retain)
        packet.flags += MQTTS_FLAG_RETAIN;
    switch (qos) {
        case -1:
          packet.flags += MQTTS_FLAG_QOS_N1;
        break;
        case 0:
          packet.flags += MQTTS_FLAG_QOS_0;
        break;
        case 1:
          packet.flags += MQTTS_FLAG_QOS_1;
        break;
        case 2:
          packet.flags += MQTTS_FLAG_QOS_2;
        break;
    }
    packet.flags += (topic_type & 0x3);
    packet.topic_id = htons(topic_id);
    packet.message_id = htons(next_message_id++);
    strncpy(packet.data, data, sizeof(packet.data));
    packet.length = 0x07 + strlen(data);

    if (debug)
        fprintf(stderr, "Sending PUBLISH packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}

void mqtts_send_subscribe(int sock, const char* topic_name, uint8_t qos)
{
    subscribe_packet_t packet;
    packet.type = MQTTS_TYPE_SUBSCRIBE;
    packet.flags = 0x00;
    switch(qos) {
      case 0: packet.flags += MQTTS_FLAG_QOS_0; break;
      case 1: packet.flags += MQTTS_FLAG_QOS_1; break;
      case 2: packet.flags += MQTTS_FLAG_QOS_2; break;
    }
    packet.message_id = htons(next_message_id++);
    strncpy(packet.topic_name, topic_name, sizeof(packet.topic_name));
    packet.topic_name[sizeof(packet.topic_name)-1] = '\0';
    packet.length = 0x05 + strlen(topic_name);

    if (debug)
        fprintf(stderr, "Sending SUBSCRIBE packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);

}

void mqtts_send_pingreq(int sock)
{
    char packet[2];

    packet[0] = 2;
    packet[1] = MQTTS_TYPE_PINGREQ;

    if (debug)
        fprintf(stderr, "Sending ping...\n");

    return send_packet(sock, (char*)&packet, 2);
}

void mqtts_send_disconnect(int sock)
{
    disconnect_packet_t packet;
    packet.type = MQTTS_TYPE_DISCONNECT;
    packet.length = 0x02;

    if (debug)
        fprintf(stderr, "Sending DISCONNECT packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}

void mqtts_recieve_connack(int sock)
{
    connack_packet_t *packet = recieve_packet(sock);

    if (packet == NULL) {
        fprintf(stderr, "Failed to connect to MQTT-S gateway.\n");
        exit(EXIT_FAILURE);
    }

    if (packet->type != MQTTS_TYPE_CONNACK) {
        fprintf(stderr, "Was expecting CONNACK packet but received: 0x%2.2x\n", packet->type);
        exit(EXIT_FAILURE);
    }

    // Check Connack return code
    if (debug)
        fprintf(stderr, "CONNACK return code: 0x%2.2x\n", packet->return_code);

    if (packet->return_code) {
        fprintf(stderr, "CONNECT error: %s\n", mqtts_return_code_string(packet->return_code));
        exit(packet->return_code);
    }
}

uint16_t mqtts_recieve_regack(int sock)
{
    regack_packet_t *packet = recieve_packet(sock);
    uint16_t received_message_id, received_topic_id;

    if (packet == NULL) {
        fprintf(stderr, "Failed to connect to register topic.\n");
        exit(EXIT_FAILURE);
    }

    if (packet->type != MQTTS_TYPE_REGACK) {
        fprintf(stderr, "Was expecting REGACK packet but received: 0x%2.2x\n", packet->type);
        exit(-1);
    }

    // Check Regack return code
    if (debug)
        fprintf(stderr, "REGACK return code: 0x%2.2x\n", packet->return_code);

    if (packet->return_code) {
        fprintf(stderr, "REGISTER error: %s\n", mqtts_return_code_string(packet->return_code));
        exit(packet->return_code);
    }

    // Check that the Message ID matches
    received_message_id = ntohs( packet->message_id );
    if (received_message_id != next_message_id-1) {
        fprintf(stderr, "Warning: message id in Regack does not equal message id sent\n");
    }

    // Return the topic ID returned by the gateway
    received_topic_id = ntohs( packet->topic_id );
    if (debug)
        fprintf(stderr, "Topic ID: %d\n", received_topic_id);

    return received_topic_id;
}

uint16_t mqtts_recieve_suback(int sock)
{
    suback_packet_t *packet = recieve_packet(sock);
    uint16_t received_message_id, received_topic_id;

    if (packet == NULL) {
        fprintf(stderr, "Failed to subscribe to topic.\n");
        exit(EXIT_FAILURE);
    }

    if (packet->type != MQTTS_TYPE_SUBACK) {
        fprintf(stderr, "Was expecting SUBACK packet but received: 0x%2.2x\n", packet->type);
        exit(-1);
    }

    // Check Suback return code
    if (debug)
        fprintf(stderr, "SUBACK return code: 0x%2.2x\n", packet->return_code);

    if (packet->return_code) {
        fprintf(stderr, "SUBSCRIBE error: %s\n", mqtts_return_code_string(packet->return_code));
        exit(packet->return_code);
    }

    // Check that the Message ID matches
    received_message_id = ntohs( packet->message_id );
    if (received_message_id != next_message_id-1) {
        fprintf(stderr, "Warning: message id in SUBACK does not equal message id sent\n");
        if (debug) {
            fprintf(stderr, "  Expecting: %d\n", next_message_id-1);
            fprintf(stderr, "  Actual: %d\n", received_message_id);
        }
    }

    // Return the topic ID returned by the gateway
    received_topic_id = ntohs( packet->topic_id );
    if (debug)
        fprintf(stderr, "Topic ID: %d\n", received_topic_id);

    return received_topic_id;
}

publish_packet_t* mqtts_loop(int sock, int timeout)
{
    time_t now = time(NULL);
    struct timeval tv;
    fd_set rfd;
    int ret;

    // Time to send a ping?
    if (keep_alive > 0 && (now - last_transmit) >= keep_alive) {
        mqtts_send_pingreq(sock);
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
        packet = recieve_packet(sock);
        if (packet) {
            if (packet[1] == MQTTS_TYPE_PUBLISH) {
                return (publish_packet_t*)packet;
            } else if (packet[1] == MQTTS_TYPE_DISCONNECT) {
                fprintf(stderr, "Warning: Received DISCONNECT from gateway.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Check for receive timeout
    if (keep_alive > 0 && (now - last_receive) >= (keep_alive * 1.5)) {
        fprintf(stderr, "Keep alive error: timed out receiving packet from gateway.\n");
        exit(EXIT_FAILURE);
    }

    return NULL;
}

const char* mqtts_type_string(uint8_t type)
{
    switch(type) {
        case MQTTS_TYPE_ADVERTISE:     return "ADVERTISE";
        case MQTTS_TYPE_SEARCHGW:      return "SEARCHGW";
        case MQTTS_TYPE_GWINFO:        return "GWINFO";
        case MQTTS_TYPE_CONNECT:       return "CONNECT";
        case MQTTS_TYPE_CONNACK:       return "CONNACK";
        case MQTTS_TYPE_WILLTOPICREQ:  return "WILLTOPICREQ";
        case MQTTS_TYPE_WILLTOPIC:     return "WILLTOPIC";
        case MQTTS_TYPE_WILLMSGREQ:    return "WILLMSGREQ";
        case MQTTS_TYPE_WILLMSG:       return "WILLMSG";
        case MQTTS_TYPE_REGISTER:      return "REGISTER";
        case MQTTS_TYPE_REGACK:        return "REGACK";
        case MQTTS_TYPE_PUBLISH:       return "PUBLISH";
        case MQTTS_TYPE_PUBACK:        return "PUBACK";
        case MQTTS_TYPE_PUBCOMP:       return "PUBCOMP";
        case MQTTS_TYPE_PUBREC:        return "PUBREC";
        case MQTTS_TYPE_PUBREL:        return "PUBREL";
        case MQTTS_TYPE_SUBSCRIBE:     return "SUBSCRIBE";
        case MQTTS_TYPE_SUBACK:        return "SUBACK";
        case MQTTS_TYPE_UNSUBSCRIBE:   return "UNSUBSCRIBE";
        case MQTTS_TYPE_UNSUBACK:      return "UNSUBACK";
        case MQTTS_TYPE_PINGREQ:       return "PINGREQ";
        case MQTTS_TYPE_PINGRESP:      return "PINGRESP";
        case MQTTS_TYPE_DISCONNECT:    return "DISCONNECT";
        case MQTTS_TYPE_WILLTOPICUPD:  return "WILLTOPICUPD";
        case MQTTS_TYPE_WILLTOPICRESP: return "WILLTOPICRESP";
        case MQTTS_TYPE_WILLMSGUPD:    return "WILLMSGUPD";
        case MQTTS_TYPE_WILLMSGRESP:   return "WILLMSGRESP";
        default:                       return "UNKNOWN";
    }
}

const char* mqtts_return_code_string(uint8_t return_code)
{
    switch(return_code) {
        case 0x00: return "Accepted";
        case 0x01: return "Rejected: congestion";
        case 0x02: return "Rejected: invalid topic ID";
        case 0x03: return "Rejected: not supported";
        default:   return "Rejected: unknown reason";
    }
}
