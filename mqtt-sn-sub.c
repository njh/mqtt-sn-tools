/*
  MQTT-SN command-line publishing client
  Copyright (C) 2013-2017 Nicholas Humfrey

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
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "mqtt-sn.h"

const char *client_id = NULL;
const char *topic_name = NULL;
const char *mqtt_sn_host = "127.0.0.1";
const char *mqtt_sn_port = MQTT_SN_DEFAULT_PORT;
uint16_t topic_id = 0;
uint16_t keep_alive = 10;
int8_t qos = 0;
uint8_t retain = FALSE;
uint8_t debug = 0;
uint8_t single_message = FALSE;
uint8_t clean_session = TRUE;
uint8_t verbose = 0;

uint8_t keep_running = TRUE;

static void usage()
{
    fprintf(stderr, "Usage: mqtt-sn-sub [opts] -t <topic>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -1             exit after receiving a single message.\n");
    fprintf(stderr, "  -c             disable 'clean session' (store subscription and pending messages when client disconnects).\n");
    fprintf(stderr, "  -d             Increase debug level by one. -d can occur multiple times.\n");
    fprintf(stderr, "  -h <host>      MQTT-SN host to connect to. Defaults to '%s'.\n", mqtt_sn_host);
    fprintf(stderr, "  -i <clientid>  ID to use for this client. Defaults to 'mqtt-sn-tools-' with process id.\n");
    fprintf(stderr, "  -k <keepalive> keep alive in seconds for this client. Defaults to %d.\n", keep_alive);
    fprintf(stderr, "  -p <port>      Network port to connect to. Defaults to %s.\n", mqtt_sn_port);
    fprintf(stderr, "  -q <qos>       QoS level to subscribe with (0 or 1). Defaults to %d.\n", qos);
    fprintf(stderr, "  -t <topic>     MQTT topic name to subscribe to.\n");
    fprintf(stderr, "  -T <topicid>   Pre-defined MQTT-SN topic ID to subscribe to.\n");
    fprintf(stderr, "  --fe           Enables Forwarder Encapsulation. Mqtt-sn packets are encapsulated according to MQTT-SN Protocol Specification v1.2, chapter 5.5 Forwarder Encapsulation.\n");
    fprintf(stderr, "  --wlnid        If Forwarder Encapsulation is enabled, wireless node ID for this client. Defaults to process id.\n");
    fprintf(stderr, "  -v             Print messages verbosely, showing the topic name.\n");
    fprintf(stderr, "  -V             Print messages verbosely, showing current time and the topic name.\n");
    exit(EXIT_FAILURE);
}

static void parse_opts(int argc, char** argv)
{

    static struct option long_options[] =
    {
        {"fe",    no_argument,       0, 'f' },
        {"wlnid", required_argument, 0, 'w' },
        {0, 0, 0, 0}
    };

    int ch;
    /* getopt_long stores the option index here. */
    int option_index = 0;

    // Parse the options/switches
    while ((ch = getopt_long(argc, argv, "1cdh:i:k:p:q:t:T:vV?", long_options, &option_index)) != -1)
        switch (ch) {
        case '1':
            single_message = TRUE;
            break;

        case 'c':
            clean_session = FALSE;
            break;

        case 'd':
            debug++;
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

        case 'q':
            qos = atoi(optarg);
            break;

        case 't':
            topic_name = optarg;
            break;

        case 'T':
            topic_id = atoi(optarg);
            break;

        case 'f':
            mqtt_sn_enable_frwdencap();
            break;

        case 'w' :
            mqtt_sn_set_frwdencap_parameters((uint8_t*)optarg, strlen(optarg));
            break;

        case 'v':
            // Prevent -v setting verbose level back down to 1 if already set to 2 by -V
            verbose = (verbose == 0) ? 1 : verbose;
            break;

        case 'V':
            verbose = 2;
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
        mqtt_sn_log_err("Please provide either a topic id or a topic name, not both.");
        exit(EXIT_FAILURE);
    }
}

static void termination_handler (int signum)
{
    switch(signum) {
    case SIGHUP:
        mqtt_sn_log_debug("Got hangup signal.");
        break;
    case SIGTERM:
        mqtt_sn_log_debug("Got termination signal.");
        break;
    case SIGINT:
        mqtt_sn_log_debug("Got interrupt signal.");
        break;
    }

    // Signal the main thread to stop
    keep_running = FALSE;
}

int main(int argc, char* argv[])
{
    int sock;

    mqtt_sn_disable_frwdencap();

    // Parse the command-line options
    parse_opts(argc, argv);

    // Enable debugging?
    mqtt_sn_set_debug(debug);
    mqtt_sn_set_verbose(verbose);
    mqtt_sn_set_timeout(keep_alive / 2);

    // Setup signal handlers
    signal(SIGTERM, termination_handler);
    signal(SIGINT, termination_handler);
    signal(SIGHUP, termination_handler);

    // Create a UDP socket
    sock = mqtt_sn_create_socket(mqtt_sn_host, mqtt_sn_port);
    if (sock) {
        // Connect to server
        mqtt_sn_log_debug("Connecting...");
        mqtt_sn_send_connect(sock, client_id, keep_alive, clean_session);
        mqtt_sn_receive_connack(sock);

        // Subscribe to the topic
        if (topic_name) {
            mqtt_sn_send_subscribe_topic_name(sock, topic_name, qos);
        } else {
            mqtt_sn_send_subscribe_topic_id(sock, topic_id, qos);
        }

        // Wait for the subscription acknowledgment
        topic_id = mqtt_sn_receive_suback(sock);
        if (topic_id && topic_name && strlen(topic_name) > 2) {
            mqtt_sn_register_topic(topic_id, topic_name);
        }

        // Keep processing packets until process is terminated
        while(keep_running) {
            publish_packet_t *packet = mqtt_sn_wait_for(MQTT_SN_TYPE_PUBLISH, sock);
            if (packet) {
                uint8_t packet_qos = packet->flags & MQTT_SN_FLAG_QOS_MASK;
                if (packet_qos == MQTT_SN_FLAG_QOS_1) {
                    mqtt_sn_send_puback(sock, packet, MQTT_SN_ACCEPTED);
                }

                if (single_message) {
                    break;
                }
            }
        }

        // Finally, disconnect
        mqtt_sn_log_debug("Disconnecting...");
        mqtt_sn_send_disconnect(sock);
        mqtt_sn_receive_disconnect(sock);

        close(sock);
    }

    mqtt_sn_cleanup();

    return 0;
}
