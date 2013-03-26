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

#include "mqtts.h"


#ifndef AI_DEFAULT
#define AI_DEFAULT (AI_ADDRCONFIG|AI_V4MAPPED)
#endif

static uint8_t debug = FALSE;
static uint16_t next_message_id = 1;


void mqtts_set_debug(uint8_t value)
{
    debug = value;
}

int mqtts_create_socket(const char* host, const char* port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
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

    return fd;
}

static void send_packet(int sock, char* data, size_t len)
{
    size_t sent = send(sock, data, len, 0);
    if (sent != len) {
        fprintf(stderr, "Warning: only sent %d of %d bytes\n", (int)sent, (int)len);
    }
}

static void* recieve_packet(int sock)
{
    static uint8_t buffer[MQTTS_MAX_PACKET_LENGTH];
    int length;
    int bytes_read;

    if (debug)
        printf("waiting for packet...\n");

    bytes_read = recv(sock, buffer, MQTTS_MAX_PACKET_LENGTH, 0);
    if (bytes_read < 0) {
        perror("recv failed");
        exit(4);
    }

    if (debug)
        printf("Received %d bytes.\n", (int)bytes_read);

    length = buffer[0];
    if (length == 0x01) {
        printf("Error: packet received is longer than this tool can handle\n");
        exit(-1);
    }

    if (length != bytes_read) {
        printf("Error: read %d bytes but packet length is %d bytes.\n", (int)bytes_read, length);
    }

    return buffer;
}

void mqtts_send_connect(int sock, const char* client_id, uint16_t keepalive)
{
    connect_packet_t packet;
    packet.type = MQTTS_TYPE_CONNECT;
    packet.flags = MQTTS_FLAG_CLEAN;
    packet.protocol_id = MQTTS_PROTOCOL_ID;
    packet.duration = htons(keepalive);

    // Generate a Client ID if none given
    if (client_id == NULL || client_id[0] == '\0') {
        snprintf(packet.client_id, sizeof(packet.client_id)-1, "mqtts-client-%d", getpid());
        packet.client_id[sizeof(packet.client_id) - 1] = '\0';
    } else {
        strncpy(packet.client_id, client_id, sizeof(packet.client_id)-1);
        packet.client_id[sizeof(packet.client_id) - 1] = '\0';
    }

    packet.length = 0x06 + strlen(packet.client_id);

    if (debug)
        printf("Sending CONNECT packet...\n");

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
        printf("Sending REGISTER packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}

void mqtts_send_publish(int sock, uint16_t topic_id, const char* data, uint8_t qos, uint8_t retain)
{
    publish_packet_t packet;
    packet.type = MQTTS_TYPE_PUBLISH;
    packet.flags = 0x00;
    if (retain)
        packet.flags += MQTTS_FLAG_RETAIN;
    packet.topic_id = htons(topic_id);
    packet.message_id = htons(next_message_id++);
    strncpy(packet.data, data, sizeof(packet.data));
    packet.length = 0x07 + strlen(data);

    if (debug)
        printf("Sending PUBLISH packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}

void mqtts_send_disconnect(int sock)
{
    disconnect_packet_t packet;
    packet.type = MQTTS_TYPE_DISCONNECT;
    packet.length = 0x02;

    if (debug)
        printf("Sending DISCONNECT packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}

void mqtts_recieve_connack(int sock)
{
    connack_packet_t *packet = recieve_packet(sock);
    uint16_t return_code;

    if (packet->type != MQTTS_TYPE_CONNACK) {
        printf("Was expecting CONNACK packet but received: 0x%2.2x\n", packet->type);
        exit(-1);
    }

    // Check Connack result code
    return_code = ntohs( packet->return_code );
    if (debug)
        printf("CONNACK result code: 0x%2.2x\n", return_code);

    if (return_code) {
        exit(return_code);
    }
}

uint16_t mqtts_recieve_regack(int sock)
{
    regack_packet_t *packet = recieve_packet(sock);
    uint16_t return_code, received_message_id, received_topic_id;

    if (packet->type != MQTTS_TYPE_REGACK) {
        printf("Was expecting REGACK packet but received: 0x%2.2x\n", packet->type);
        exit(-1);
    }

    // Check Regack result code
    return_code = ntohs( packet->return_code );
    if (debug)
        printf("REGACK result code: 0x%2.2x\n", return_code);

    if (return_code) {
        exit(return_code);
    }

    // Check that the Message ID matches
    received_message_id = ntohs( packet->message_id );
    if (received_message_id != next_message_id-1) {
        printf("Warning: message id in Regack does not equal message id sent\n");
    }

    // Store the topic ID returned by the gateway
    received_topic_id = ntohs( packet->topic_id );
    if (debug)
        printf("Topic ID: %d\n", received_topic_id);

    return received_topic_id;
}
