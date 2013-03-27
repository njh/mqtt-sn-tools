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
#include <netdb.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "mqtts.h"

const char *client_id = NULL;
const char *topic_name = NULL;
const char *mqtts_host = "127.0.0.1";
const char *mqtts_port = "1883";
uint16_t keep_alive = 0;
uint16_t topic_id = 0;
uint8_t retain = FALSE;
uint8_t debug = FALSE;
uint8_t single_message = FALSE;
uint8_t clean_session = TRUE;
uint8_t verbose = FALSE;


static void usage()
{
    fprintf(stderr, "Usage: mqtts-sub [opts] -t <topic>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -1             exit after receiving a single message.\n");
    fprintf(stderr, "  -c             disable 'clean session' (store subscription and pending messages when client disconnects).\n");
    fprintf(stderr, "  -d             Enable debug messages.\n");
    fprintf(stderr, "  -h <host>      MQTT-S host to connect to. Defaults to '%s'.\n", mqtts_host);
    fprintf(stderr, "  -i <clientid>  ID to use for this client. Defaults to 'mqtts-client-$$'.\n");
    fprintf(stderr, "  -k <keepalive> keep alive in seconds for this client. Defaults to %d.\n", keep_alive);
    fprintf(stderr, "  -p <port>      Network port to connect to. Defaults to %s.\n", mqtts_port);
    fprintf(stderr, "  -t <topic>     MQTT topic name to subscribe to.\n");
    fprintf(stderr, "  -v             print published messages verbosely.\n");
    exit(-1);
}

static void parse_opts(int argc, char** argv)
{
    int ch;

    // Parse the options/switches
    while ((ch = getopt(argc, argv, "1cdh:i:k:p:t:v?")) != -1)
        switch (ch) {
        case '1':
            single_message = TRUE;
        break;

        case 'c':
            clean_session = FALSE;
        break;

        case 'd':
            debug = TRUE;
        break;

        case 'h':
            mqtts_host = optarg;
        break;

        case 'i':
            client_id = optarg;
        break;

        case 'k':
            keep_alive = atoi(optarg);
        break;

        case 'p':
            mqtts_port = optarg;
        break;

        case 't':
            topic_name = optarg;
        break;

        case 'v':
            verbose = TRUE;
        break;

        case '?':
        default:
            usage();
        break;
    }

    // Missing Parameter?
    if (!topic_name) {
        usage();
    }
}

int main(int argc, char* argv[])
{
    int sock;

    // Parse the command-line options
    parse_opts(argc, argv);
    
    // Enable debugging?
    mqtts_set_debug(debug);

    // Create a UDP socket
    sock = mqtts_create_socket(mqtts_host, mqtts_port);
    if (sock) {
        // Connect to gateway
        mqtts_send_connect(sock, client_id, keep_alive);
        mqtts_recieve_connack(sock);

        // Subscribe to the topic
        mqtts_send_subscribe(sock, topic_name, 0);
        topic_id = mqtts_recieve_suback(sock);
        
        while(1) {
            publish_packet_t *packet = mqtts_recieve_publish(sock);
            if (packet) {
                printf("%s\n", packet->data);
            }
        }

        // Finally, disconnect
        mqtts_send_disconnect(sock);

        close(sock);
    }

    return 0;
}
