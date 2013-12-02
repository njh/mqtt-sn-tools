/*
  MQTT-SN command-line publishing client
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
#include <signal.h>

#include "mqtt-sn.h"

const char *client_id = NULL;
const char *topic_name = NULL;
const char *mqtt_sn_host = "127.0.0.1";
const char *mqtt_sn_port = "1883";
uint16_t keep_alive = 10;
uint16_t topic_id = 0;
uint8_t retain = FALSE;
uint8_t debug = FALSE;
uint8_t single_message = FALSE;
uint8_t clean_session = TRUE;

uint8_t keep_running = TRUE;

static void usage()
{
    fprintf(stderr, "Usage: mqtt-sn-sub [opts] -t <topic>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -1             exit after receiving a single message.\n");
    fprintf(stderr, "  -c             disable 'clean session' (store subscription and pending messages when client disconnects).\n");
    fprintf(stderr, "  -d             Enable debug messages.\n");
    fprintf(stderr, "  -h <host>      MQTT-SN host to connect to. Defaults to '%s'.\n", mqtt_sn_host);
    fprintf(stderr, "  -i <clientid>  ID to use for this client. Defaults to 'mqtt-sn-tools-' with process id.\n");
    fprintf(stderr, "  -k <keepalive> keep alive in seconds for this client. Defaults to %d.\n", keep_alive);
    fprintf(stderr, "  -p <port>      Network port to connect to. Defaults to %s.\n", mqtt_sn_port);
    fprintf(stderr, "  -t <topic>     MQTT topic name to subscribe to.\n");
    exit(-1);
}

static void parse_opts(int argc, char** argv)
{
    int ch;

    // Parse the options/switches
    while ((ch = getopt(argc, argv, "1cdh:i:k:p:t:?")) != -1)
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
            mqtt_sn_host = optarg;
        break;

        case 'i':
            client_id = optarg;
        break;

        case 'k':
            keep_alive = atoi(optarg);
        break;

        case 'p':
            mqtt_sn_port = optarg;
        break;

        case 't':
            topic_name = optarg;
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

static void termination_handler (int signum)
{
    switch(signum) {
        case SIGHUP:  fprintf(stderr, "Got hangup signal."); break;
        case SIGTERM: fprintf(stderr, "Got termination signal."); break;
        case SIGINT:  fprintf(stderr, "Got interupt signal."); break;
    }

    // Signal the main thead to stop
    keep_running = FALSE;
}

int main(int argc, char* argv[])
{
    int sock, timeout;

    // Parse the command-line options
    parse_opts(argc, argv);
    
    // Enable debugging?
    mqtt_sn_set_debug(debug);

    // Work out timeout value for main loop
    if (keep_alive) {
        timeout = keep_alive / 2;
    } else {
        timeout = 10;
    }

    // Setup signal handlers
    signal(SIGTERM, termination_handler);
    signal(SIGINT, termination_handler);
    signal(SIGHUP, termination_handler);

    // Create a UDP socket
    sock = mqtt_sn_create_socket(mqtt_sn_host, mqtt_sn_port);
    if (sock) {
        // Connect to gateway
        mqtt_sn_send_connect(sock, client_id, keep_alive);
        mqtt_sn_recieve_connack(sock);

        // Subscribe to the topic
        mqtt_sn_send_subscribe(sock, topic_name, 0);
        topic_id = mqtt_sn_recieve_suback(sock);

        // Keep processing packets until process is terminated
        while(keep_running) {
            publish_packet_t *packet = mqtt_sn_loop(sock, timeout);
            if (packet) {
                printf("%s\n", packet->data);

                // Stop if in single message mode
                if (single_message)
                    break;
            }
        }

        // Finally, disconnect
        mqtt_sn_send_disconnect(sock);

        close(sock);
    }

    return 0;
}
