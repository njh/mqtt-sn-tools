/*
  MQTT-SN command-line publishing client
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
#include <signal.h>

#include "mqtt-sn.h"

const char *client_id = NULL;
const char *topic_name = NULL;
const char *mqtt_sn_host = "127.0.0.1";
const char *mqtt_sn_port = "1883";
uint16_t topic_id = 0;
uint16_t keep_alive = 10;
uint8_t retain = FALSE;
uint8_t debug = FALSE;
uint8_t single_message = FALSE;
uint8_t clean_session = TRUE;
uint8_t verbose = FALSE;

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
    fprintf(stderr, "  -T <topicid>   Pre-defined MQTT-SN topic ID to subscrube to.\n");
    fprintf(stderr, "  -v             Print messages verbosely, showing the topic name.\n");
    exit(-1);
}

static void parse_opts(int argc, char** argv)
{
    int ch;

    // Parse the options/switches
    while ((ch = getopt(argc, argv, "1cdh:i:k:p:t:T:v?")) != -1)
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

        case 'T':
            topic_id = atoi(optarg);
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
    if (!topic_name && !topic_id) {
        usage();
    }

    // Both topic name and topic id?
    if (topic_name && topic_id) {
        fprintf(stderr, "Error: please provide either a topic id or a topic name, not both.\n");
        exit(-1);
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
        mqtt_sn_receive_connack(sock);

        // Subscribe to the topic
        if (topic_name) {
            mqtt_sn_send_subscribe_topic_name(sock, topic_name, 0);
        } else {
            mqtt_sn_send_subscribe_topic_id(sock, topic_id, 0);
        }

        // Wait for the subscription acknowledgement
        topic_id = mqtt_sn_receive_suback(sock);
        if (topic_id) {
            mqtt_sn_register_topic(topic_id, topic_name);
        }

        // Keep processing packets until process is terminated
        while(keep_running) {
            publish_packet_t *packet = mqtt_sn_loop(sock, timeout);
            if (packet) {
                if (verbose) {
                    int topic_type = packet->flags & 0x3;
                    int topic_id = ntohs(packet->topic_id);
                    switch (topic_type) {
                        case MQTT_SN_TOPIC_TYPE_NORMAL: {
                            const char *topic_name = mqtt_sn_lookup_topic(topic_id);
                            printf("%s: %s\n", topic_name, packet->data);
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

                // Stop if in single message mode
                if (single_message)
                    break;
            }
        }

        // Finally, disconnect
        mqtt_sn_send_disconnect(sock);
        mqtt_sn_receive_disconnect(sock);

        close(sock);
    }

    mqtt_sn_cleanup();

    return 0;
}
