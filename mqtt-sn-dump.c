/*
  MQTT-SN command-line dump
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "mqtt-sn.h"

const char *mqtt_sn_port = MQTT_SN_DEFAULT_PORT;
uint8_t dump_all = FALSE;
uint8_t debug = 0;
uint8_t verbose = 0;
uint8_t keep_running = TRUE;


static void usage()
{
    fprintf(stderr, "Usage: mqtt-sn-dump [opts] -p <port>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -a             Dump all packet types.\n");
    fprintf(stderr, "  -d             Increase debug level by one. -d can occur multiple times.\n");
    fprintf(stderr, "  -p <port>      Network port to listen on. Defaults to %s.\n", mqtt_sn_port);
    fprintf(stderr, "  -v             Print messages verbosely, showing the topic name.\n");
    exit(EXIT_FAILURE);
}

static void parse_opts(int argc, char** argv)
{
    int ch;

    // Parse the options/switches
    while((ch = getopt(argc, argv, "adp:v?")) != -1)
        switch(ch) {
            case 'a':
                dump_all = TRUE;
                break;

            case 'd':
                debug++;
                break;

            case 'p':
                mqtt_sn_port = optarg;
                break;

            case 'v':
                verbose++;
                break;

            case '?':
            default:
                usage();
                break;
        }
}

static int bind_udp_socket(const char* port_str)
{
    struct sockaddr_in si_me;
    short port = atoi(port_str);
    int sock;

    if ((sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (const struct sockaddr *)&si_me, sizeof(si_me)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    mqtt_sn_log_debug("mqtt-sn-dump listening on port %s", port_str);

    return sock;
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

    // Disable buffering on STDOUT
    setvbuf(stdout, NULL, _IONBF, 0);

    // Parse the command-line options
    parse_opts(argc, argv);

    // Enable debugging?
    mqtt_sn_set_debug(debug);
    mqtt_sn_set_verbose(verbose);

    // Setup signal handlers
    signal(SIGTERM, termination_handler);
    signal(SIGINT, termination_handler);
    signal(SIGHUP, termination_handler);

    // Create a listening UDP socket
    sock = bind_udp_socket(mqtt_sn_port);

    while (keep_running) {
        int ret = mqtt_sn_select(sock);
        if (ret < 0) {
            break;
        } else if (ret > 0) {
            char* packet = mqtt_sn_receive_packet(sock);
            if (dump_all) {
                mqtt_sn_dump_packet(packet);
            } else if (packet[1] == MQTT_SN_TYPE_PUBLISH) {
                mqtt_sn_print_publish_packet((publish_packet_t *)packet);
            }
        }
    }

    close(sock);

    return 0;
}

