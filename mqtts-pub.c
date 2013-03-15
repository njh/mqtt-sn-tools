/*
  MQTT-S command-line publishing client
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdint.h>

#include "mqtts.h"


#define DEFAULT_PORT 1883
#define DEFAULT_SERVER "127.0.0.1"

char *client_id = "client-id";
char *topic_name = "test";
char *data = "Hello World";
uint16_t keep_alive = 0;
uint16_t topic_id = 1;
uint16_t next_message_id = 1;

int debug = 1;


int create_socket()
{
    struct sockaddr_in addr;
    int fd, ret;

    // Create a UDP socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("Error creating socket");
        exit(1);
    }

    // Set the destination address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(DEFAULT_SERVER);
    addr.sin_port = htons(DEFAULT_PORT);

    // FIXME: set the Don't Fragment flag

    // Connect socket to the remote host
    ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret) {
        perror("Error connecting socket");
        exit(1);
    }

    return fd;
}

void send_packet(int sock, char* data, size_t len)
{
    size_t sent = send(sock, data, len, 0);
    if (sent != len) {
        fprintf(stderr, "Warning: only sent %d of %d bytes\n", (int)sent, (int)len);
    }
}

void* recieve_packet(int sock)
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

void send_connect(int sock, const char* client_id)
{
    connect_packet_t packet;
    packet.type = MQTTS_TYPE_CONNECT;
    packet.flags = MQTTS_FLAG_CLEAN;
    packet.protocol_id = MQTTS_PROTOCOL_ID;
    packet.duration = htons(keep_alive);
    strncpy(packet.client_id, client_id, sizeof(packet.client_id));
    packet.length = 0x06 + strlen(packet.client_id);

    if (debug)
        printf("Sending CONNECT packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}

void send_register(int sock, uint16_t topic_id, const char* topic_name)
{
    register_packet_t packet;
    packet.type = MQTTS_TYPE_REGISTER;
    packet.topic_id = htons(topic_id);
    packet.message_id = htons(next_message_id++);
    strncpy(packet.topic_name, topic_name, sizeof(packet.topic_name));
    packet.length = 0x06 + strlen(packet.topic_name);

    if (debug)
        printf("Sending REGISTER packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}

void send_publish(int sock, uint16_t topic_id, const char* data)
{
    publish_packet_t packet;
    packet.type = MQTTS_TYPE_PUBLISH;
    packet.flags = 0x00;
    packet.topic_id = htons(topic_id);
    packet.message_id = htons(next_message_id++);
    strncpy(packet.data, data, sizeof(packet.data));
    packet.length = 0x07 + strlen(data);

    if (debug)
        printf("Sending PUBLISH packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}

void recieve_connack(int sock)
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

void recieve_regack(int sock, uint16_t topic_id)
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

    // Check that the Message ID matches
    received_message_id = ntohs( packet->message_id );
    if (received_message_id != next_message_id-1) {
        printf("Warning: message id in Regack does not equal message id sent\n");
    }

    // Check that the Topic ID matches
    received_topic_id = ntohs( packet->topic_id );
    if (received_topic_id != topic_id) {
        printf("Warning: topic id in Regack does not equal topic id sent\n");
    }

    if (return_code) {
        exit(return_code);
    }
}

int main(int arvc, char* argv[])
{
    int sock = create_socket();
    if (sock) {
        // Connect to gateway
        send_connect(sock, client_id);
        recieve_connack(sock);

        // Register the topic
        send_register(sock, topic_id, topic_name);
        recieve_regack(sock, topic_id);

        // Publish to the topic
        send_publish(sock, topic_id, data);

        close(sock);
    }

    return 0;
}
