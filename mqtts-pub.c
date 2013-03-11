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

#include "mqtts.h"

#define DEFAULT_PORT 1883
#define DEFAULT_SERVER "127.0.0.1"
#define MAX_LENGTH   1440


char *client_id = "mqtts-client-pub";
char *topic_name = "test";
uint16_t keep_alive = 0;
uint16_t topic_id = 1;


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

int send_packet(int sock, char* data, size_t len)
{
    size_t sent = send(sock, data, len, 0);
    printf("Sent %u bytes\n", (unsigned int)sent);
    return 0;
}

int send_connect(int sock)
{
    connect_packet_t packet;
    packet.type = MQTTS_TYPE_CONNECT;
    packet.flags = MQTTS_FLAG_CLEAN;
    packet.protocol_id = MQTTS_PROTOCOL_ID;
    packet.duration = keep_alive;
    strncpy(packet.client_id, client_id, sizeof(packet.client_id));
    packet.length = 0x06 + strlen(packet.client_id);
    return send_packet(sock, (char*)&packet, packet.length);
}

int main(int arvc, char* argv[])
{
    int sock = create_socket();

    if (sock) {
        send_connect(sock);
        close(sock);
    }

    return 0;
}
