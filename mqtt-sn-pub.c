/*
  MQTT-SN command-line publishing client
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
#include <getopt.h>
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
const char *message_data = NULL;
const char *message_file = NULL;
const char *mqtt_sn_host = "127.0.0.1";
const char *mqtt_sn_port = MQTT_SN_DEFAULT_PORT;
uint16_t source_port = 0;
uint16_t topic_id = 0;
uint8_t topic_id_type = MQTT_SN_TOPIC_TYPE_NORMAL;
uint16_t keep_alive = MQTT_SN_DEFAULT_KEEP_ALIVE;
uint16_t sleep_duration = 0;
int8_t qos = 0;
uint8_t retain = FALSE;
uint8_t one_message_per_line = FALSE;
uint8_t debug = 0;


static void usage()
{
    fprintf(stderr, "Usage: mqtt-sn-pub [opts] -t <topic> -m <message>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -d             Increase debug level by one. -d can occur multiple times.\n");
    fprintf(stderr, "  -f <file>      A file to send as the message payload.\n");
    fprintf(stderr, "  -h <host>      MQTT-SN host to connect to. Defaults to '%s'.\n", mqtt_sn_host);
    fprintf(stderr, "  -i <clientid>  ID to use for this client. Defaults to 'mqtt-sn-tools-' with process id.\n");
    fprintf(stderr, "  -k <keepalive> keep alive in seconds for this client. Defaults to %d.\n", keep_alive);
    fprintf(stderr, "  -e <sleep>     sleep duration in seconds when disconnecting. Defaults to %d.\n", sleep_duration);
    fprintf(stderr, "  -m <message>   Message payload to send.\n");
    fprintf(stderr, "  -l             Read from STDIN, one message per line.\n");
    fprintf(stderr, "  -n             Send a null (zero length) message.\n");
    fprintf(stderr, "  -p <port>      Network port to connect to. Defaults to %s.\n", mqtt_sn_port);
    fprintf(stderr, "  -q <qos>       Quality of Service value (0, 1 or -1). Defaults to %d.\n", qos);
    fprintf(stderr, "  -r             Message should be retained.\n");
    fprintf(stderr, "  -s             Read one whole message from STDIN.\n");
    fprintf(stderr, "  -t <topic>     MQTT-SN topic name to publish to.\n");
    fprintf(stderr, "  -T <topicid>   Pre-defined MQTT-SN topic ID to publish to.\n");
    fprintf(stderr, "  --fe           Enables Forwarder Encapsulation. Mqtt-sn packets are encapsulated according to MQTT-SN Protocol Specification v1.2, chapter 5.5 Forwarder Encapsulation.\n");
    fprintf(stderr, "  --wlnid        If Forwarder Encapsulation is enabled, wireless node ID for this client. Defaults to process id.\n");
    fprintf(stderr, "  --cport <port> Source port for outgoing packets. Uses port in ephemeral range if not specified or set to %d.\n", source_port);
    exit(EXIT_FAILURE);
}

static void parse_opts(int argc, char** argv)
{

    static struct option long_options[] = {
        {"fe",    no_argument,       0, 1000 },
        {"wlnid", required_argument, 0, 1001 },
        {"cport", required_argument, 0, 1002 },
        {0, 0, 0, 0}
    };

    int ch;
    /* getopt_long stores the option index here. */
    int option_index = 0;

    // Parse the options/switches
    while ((ch = getopt_long (argc, argv, "df:h:i:k:e:lm:np:q:rst:T:?", long_options, &option_index)) != -1) {
        switch (ch) {
            case 'd':
                debug++;
                break;

            case 'h':
                mqtt_sn_host = optarg;
                break;

            case 'f':
                message_file = optarg;
                break;

            case 'i':
                client_id = optarg;
                break;

            case 'l':
                message_file = "-";
                one_message_per_line = TRUE;
                break;

            case 'm':
                message_data = optarg;
                break;

            case 'k':
                keep_alive = atoi(optarg);
                break;

            case 'e':
                sleep_duration = atoi(optarg);
                break;

            case 'n':
                message_data = "";
                break;

            case 'p':
                mqtt_sn_port = optarg;
                break;

            case 'q':
                qos = atoi(optarg);
                break;

            case 'r':
                retain = TRUE;
                break;

            case 's':
                message_file = "-";
                one_message_per_line = FALSE;
                break;

            case 't':
                topic_name = optarg;
                break;

            case 'T':
                topic_id = atoi(optarg);
                break;

            case 1000:
                mqtt_sn_enable_frwdencap();
                break;

            case 1001:
                mqtt_sn_set_frwdencap_parameters((uint8_t*)optarg, strlen(optarg));
                break;

            case 1002:
                source_port = atoi(optarg);
                break;

            case '?':
            default:
                usage();
                break;
        } // switch
    } // while

    // Missing Parameter?
    if (!(topic_name || topic_id) || !(message_data || message_file)) {
        usage();
    }

    if (qos != -1 && qos != 0 && qos != 1) {
        mqtt_sn_log_err("Only QoS level 0, 1 or -1 is supported.");
        exit(EXIT_FAILURE);
    }

    // Both topic name and topic id?
    if (topic_name && topic_id) {
        mqtt_sn_log_err("Please provide either a topic id or a topic name, not both.");
        exit(EXIT_FAILURE);
    }

    // Both message data and file?
    if (message_data && message_file) {
        mqtt_sn_log_err("Please provide either message data or a message file, not both.");
        exit(EXIT_FAILURE);
    }

    // Check topic is valid for QoS level -1
    if (qos == -1 && topic_id == 0 && strlen(topic_name) != 2) {
        mqtt_sn_log_err("Either a pre-defined topic id or a short topic name must be given for QoS -1.");
        exit(EXIT_FAILURE);
    }
}

static void publish_file(int sock, const char* filename)
{
    char buffer[MQTT_SN_MAX_PAYLOAD_LENGTH];
    uint16_t message_len = 0;
    FILE* file = NULL;

    if (strcmp(filename, "-") == 0) {
        file = stdin;
    } else {
        file = fopen(filename, "rb");
    }

    if (!file) {
        perror("Failed to open message file");
        exit(EXIT_FAILURE);
    }

    if (one_message_per_line) {
        // One message per line
        while(!feof(file) && !ferror(file)) {
            char* message = fgets(buffer, MQTT_SN_MAX_PAYLOAD_LENGTH, file);
            if (message) {
                char* end = strpbrk(message, "\n\r");
                if (end) {
                    uint16_t message_len = (end - message);
                    mqtt_sn_send_publish(sock, topic_id, topic_id_type, message, message_len, qos, retain);
                } else {
                    mqtt_sn_log_err("Failed to find newline when reading message");
                }
            } else if (ferror(file)) {
                perror("Failed to read message line");
                exit(EXIT_FAILURE);
            }
        }
    } else {
        // One message until EOF
        message_len = fread(buffer, 1, MQTT_SN_MAX_PAYLOAD_LENGTH, file);
        if (ferror(file)) {
            perror("Failed to read message file");
            exit(EXIT_FAILURE);
        } else if (!feof(file)) {
            mqtt_sn_log_warn("Input file is longer than the maximum message size");
        }

        mqtt_sn_send_publish(sock, topic_id, topic_id_type, buffer, message_len, qos, retain);
    }

    fclose(file);
}

int main(int argc, char* argv[])
{
    int sock;

    mqtt_sn_disable_frwdencap();

    // Parse the command-line options
    parse_opts(argc, argv);

    // Enable debugging?
    mqtt_sn_set_debug(debug);
    mqtt_sn_set_timeout(keep_alive / 2);

    // Create a UDP socket
    sock = mqtt_sn_create_socket(mqtt_sn_host, mqtt_sn_port, source_port);
    if (sock) {
        // Connect to gateway
        if (qos >= 0) {
            mqtt_sn_log_debug("Connecting...");
            mqtt_sn_send_connect(sock, client_id, keep_alive, TRUE);
            mqtt_sn_receive_connack(sock);
        }

        if (topic_id) {
            // Use pre-defined topic ID
            topic_id_type = MQTT_SN_TOPIC_TYPE_PREDEFINED;
        } else if (strlen(topic_name) == 2) {
            // Convert the 2 character topic name into a 2 byte topic id
            topic_id = (topic_name[0] << 8) + topic_name[1];
            topic_id_type = MQTT_SN_TOPIC_TYPE_SHORT;
        } else if (qos >= 0) {
            // Register the topic name
            mqtt_sn_send_register(sock, topic_name);
            topic_id = mqtt_sn_receive_regack(sock);
            topic_id_type = MQTT_SN_TOPIC_TYPE_NORMAL;
        }

        // Publish to the topic
        if (message_file) {
            publish_file(sock, message_file);
        } else {
            uint16_t message_len = strlen(message_data);
            mqtt_sn_send_publish(sock, topic_id, topic_id_type, message_data, message_len, qos, retain);
        }

        // Finally, disconnect
        if (qos >= 0) {
            mqtt_sn_log_debug("Disconnecting...");
            mqtt_sn_send_disconnect(sock, sleep_duration);
            mqtt_sn_receive_disconnect(sock);
        }

        close(sock);
    }

    mqtt_sn_cleanup();

    return 0;
}
