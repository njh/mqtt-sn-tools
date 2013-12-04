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
const char *message_data = NULL;
time_t keep_alive = 30;
const char *mqtt_sn_host = "127.0.0.1";
const char *mqtt_sn_port = "1883";
uint16_t topic_id = 0;
uint8_t topic_id_type = MQTT_SN_TOPIC_TYPE_NORMAL;
int8_t qos = 0;
uint8_t retain = FALSE;
uint8_t debug = FALSE;


static void usage()
{
    fprintf(stderr, "Usage: mqtt-sn-pub [opts] -t <topic> -m <message>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -d             Enable debug messages.\n");
    fprintf(stderr, "  -h <host>      MQTT-SN host to connect to. Defaults to '%s'.\n", mqtt_sn_host);
    fprintf(stderr, "  -i <clientid>  ID to use for this client. Defaults to 'mqtt-sn-tools-' with process id.\n");
    fprintf(stderr, "  -m <message>   Message payload to send.\n");
    fprintf(stderr, "  -n             Send a null (zero length) message.\n");
    fprintf(stderr, "  -p <port>      Network port to connect to. Defaults to %s.\n", mqtt_sn_port);
    fprintf(stderr, "  -q <qos>       Quality of Service value (0 or -1). Defaults to %d.\n", qos);
    fprintf(stderr, "  -r             Message should be retained.\n");
    fprintf(stderr, "  -t <topic>     MQTT topic name to publish to.\n");
    fprintf(stderr, "  -T <topicid>   Pre-defined MQTT-SN topic ID to publish to.\n");
    exit(-1);
}

static void parse_opts(int argc, char** argv)
{
    int ch;

    // Parse the options/switches
    while ((ch = getopt(argc, argv, "dh:i:m:np:q:rt:T:?")) != -1)
        switch (ch) {
        case 'd':
            debug = TRUE;
        break;

        case 'h':
            mqtt_sn_host = optarg;
        break;

        case 'i':
            client_id = optarg;
        break;

        case 'm':
            message_data = optarg;
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

        case 't':
            topic_name = optarg;
        break;

        case 'T':
            topic_id = atoi(optarg);
        break;

        case '?':
        default:
            usage();
        break;
    }

    // Missing Parameter?
    if (!(topic_name || topic_id) || !message_data) {
        usage();
    }
    
    if (qos != -1 && qos != 0) {
        fprintf(stderr, "Error: only QoS level 0 or -1 is supported.\n");
        exit(-1);
    }

    // Both topic name and topic id?
    if (topic_name && topic_id) {
        fprintf(stderr, "Error: please provide either a topic id or a topic name, not both.\n");
        exit(-1);
    }

    // Check topic is valid for QoS level -1
    if (qos == -1 && topic_id == 0 && strlen(topic_name) != 2) {
        fprintf(stderr, "Error: either a pre-defined topic id or a short topic name must be given for QoS -1.\n");
        exit(-1);
    }
}

int main(int argc, char* argv[])
{
    int sock;

    // Parse the command-line options
    parse_opts(argc, argv);

    // Enable debugging?
    mqtt_sn_set_debug(debug);

    // Create a UDP socket
    sock = mqtt_sn_create_socket(mqtt_sn_host, mqtt_sn_port);
    if (sock) {
        // Connect to gateway
        if (qos >= 0) {
            mqtt_sn_send_connect(sock, client_id, keep_alive);
            mqtt_sn_recieve_connack(sock);
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
            topic_id = mqtt_sn_recieve_regack(sock);
            topic_id_type = MQTT_SN_TOPIC_TYPE_NORMAL;
        }

        // Publish to the topic
        mqtt_sn_send_publish(sock, topic_id, topic_id_type, message_data, qos, retain);

        // Finally, disconnect
        if (qos >= 0) {
            mqtt_sn_send_disconnect(sock);
        }

        close(sock);
    }
 
    mqtt_sn_cleanup();

    return 0;
}
