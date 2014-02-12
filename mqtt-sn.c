/*
  Basic MQTT-SN client library
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
#include "simple-udp.h"
#include "net/uip.h"


#include <string.h>
#include <stdint.h>

//#include <unistd.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netdb.h>
//#include <time.h>
//#include <errno.h>

#include "mqtt-sn.h"


#ifndef AI_DEFAULT
#define AI_DEFAULT (AI_ADDRCONFIG|AI_V4MAPPED)
#endif

static uint8_t debug = FALSE;
static uint16_t next_message_id = 1;
static time_t last_transmit = 0;
static time_t last_receive = 0;
static time_t keep_alive = 0;

topic_map_t *topic_map = NULL;

/*MQTT_SN events*/
//Ping request
//static process_event_t pingreq_event;
//Ping response
//static process_event_t pingresp_event;
//PubAck received
//static process_event_t puback_event;
//Connack recieved
static process_event_t connack_event;
//Regack received
//static process_event_t regack_event;
//Willtopic request received
//Willmsg request received
//WillTopicResp received
//WillMsgResp received
//Disconnect received
static process_event_t disconnect_event;
static process_event_t receive_timeout_event;
static process_event_t send_timeout_event;

PROCESS(mqtt_sn_process, "MQTT_SN process");
//AUTOSTART_PROCESSES(&mqtt_sn_process);

#if 1
void mqtt_sn_set_debug(uint8_t value)
{
    debug = value;
}
#endif

#if 1
int mqtt_sn_create_socket(struct mqtt_sn_connection *mqc, uint16_t local_port, uip_ipaddr_t *remote_addr, uint16_t remote_port)
{
  simple_udp_register(&(mqc->sock), local_port, remote_addr, remote_port, mqtt_sn_receiver);
  mqc->stat = DISCONNECTED;
  mqc->keep_alive=0;
  mqc->next_message_id = 1;
  process_start(&mqtt_sn_process, NULL);
  return 0;
}
#endif

#if 1
//static connack_packet_t last_connack;
//static regack_packet_t last_regack;
//static puback_packet_t last_puback;
//static disconnect_packet_t last_disconnect;
static void
mqtt_sn_receiver(struct simple_udp_connection *sock, const uip_ipaddr_t *sender_addr, uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr, uint16_t receiver_port, const uint8_t *data, uint16_t datalen)
{
  uint8_t msg_type;
  struct mqtt_sn_connection *mqc = (struct mqtt_sn_connection *)sock;
  if (mqc->keep_alive > 0 && mqc->stat == CONNECTED){
    ctimer_restart((&(mqc->receive_timer)));
  }
  //last_receive = clock_time();
  if (datalen >= 2)
  {
    msg_type = data[1];
    switch(msg_type) {
//        case MQTT_SN_TYPE_ADVERTISE:
//        case MQTT_SN_TYPE_SEARCHGW:
//        case MQTT_SN_TYPE_GWINFO:
//        case MQTT_SN_TYPE_CONNECT:
        case MQTT_SN_TYPE_CONNACK:
          {
            memcpy(&(mqc->last_connack),data,sizeof(connack_packet_t));
            process_post(&mqtt_sn_process, connack_event, mqc);
            if(mqc->mc->connack_recv != NULL) {
              mqc->mc->connack_recv(mqc, receiver_addr, data, datalen);
            }
            break;
          }
//        case MQTT_SN_TYPE_WILLTOPICREQ:
//        case MQTT_SN_TYPE_WILLTOPIC:
//        case MQTT_SN_TYPE_WILLMSGREQ:
//        case MQTT_SN_TYPE_WILLMSG:
//        case MQTT_SN_TYPE_REGISTER:
        case MQTT_SN_TYPE_REGACK:
          {
            memcpy(&mqc->last_regack,data,sizeof(regack_packet_t));
            if(mqc->mc->regack_recv != NULL) {
              mqc->mc->regack_recv(mqc, receiver_addr, data, datalen);
            }
            break;
          }
//        case MQTT_SN_TYPE_PUBLISH:
        case MQTT_SN_TYPE_PUBACK:
          {
            memcpy(&mqc->last_puback,data,sizeof(puback_packet_t));
            if(mqc->mc->puback_recv != NULL) {
              mqc->mc->puback_recv(mqc, receiver_addr, data, datalen);
            }
            break;
          }
//        case MQTT_SN_TYPE_PUBCOMP:
//        case MQTT_SN_TYPE_PUBREC:
//        case MQTT_SN_TYPE_PUBREL:
//        case MQTT_SN_TYPE_SUBSCRIBE:
//        case MQTT_SN_TYPE_SUBACK:
//        case MQTT_SN_TYPE_UNSUBSCRIBE:
//        case MQTT_SN_TYPE_UNSUBACK:
        case MQTT_SN_TYPE_PINGREQ:
          {
            mqtt_sn_send_pingresp(mqc);
            if(mqc->mc->pingreq_recv != NULL) {
              mqc->mc->pingreq_recv(mqc, receiver_addr, data, datalen);
            }
            break;
          }
        case MQTT_SN_TYPE_PINGRESP:
          {
            if(mqc->mc->pingresp_recv != NULL) {
              mqc->mc->pingresp_recv(mqc, receiver_addr, data, datalen);
            }
            break;
          }
        case MQTT_SN_TYPE_DISCONNECT:
          {
            memcpy(&mqc->last_disconnect,data,sizeof(disconnect_packet_t));
            process_post(&mqtt_sn_process, disconnect_event, mqc);
            if(mqc->mc->disconnect_recv != NULL) {
              mqc->mc->disconnect_recv(mqc, receiver_addr, data, datalen);
            }
            break;
          }
//        case MQTT_SN_TYPE_WILLTOPICUPD:
//        case MQTT_SN_TYPE_WILLTOPICRESP:
//        case MQTT_SN_TYPE_WILLMSGUPD:
//        case MQTT_SN_TYPE_WILLMSGRESP:
        default:
          {
            if (debug) {
              printf(" unrecognized received packet...\n");
            }
            break;
          }
    }
  }
}
#endif



static void receive_timer_callback(void *mqc)
{
  process_post(&mqtt_sn_process, receive_timeout_event, mqc);
}
static void send_timer_callback(void *mqc)
{
  process_post(&mqtt_sn_process, send_timeout_event, mqc);
}
PROCESS_THREAD(mqtt_sn_process, ev, data)
{
  struct mqtt_sn_connection *mqc;

  PROCESS_BEGIN();

  connack_event = process_alloc_event();
  disconnect_event = process_alloc_event();
  receive_timeout_event = process_alloc_event();
  send_timeout_event = process_alloc_event();

  while(1) {
    PROCESS_WAIT_EVENT();
    mqc = (struct mqtt_sn_connection *)data;
    if(ev == connack_event) {
      //if connection was succesful, set and or start the ctimers.
      if (mqc->last_connack.return_code == 0x00 && mqc->stat == WAITING_ACK){
        mqc->stat = CONNECTED;
        if (mqc->keep_alive > 0){
          ctimer_set(&mqc->receive_timer, mqc->keep_alive * 2, receive_timer_callback, mqc);
          ctimer_set(&mqc->send_timer,mqc->keep_alive / 2,send_timer_callback, mqc);
        }
      }
    }
    else if (ev == disconnect_event){
      //stop the timers
      if (mqc->keep_alive > 0){
        ctimer_stop(&(mqc->receive_timer));
        ctimer_stop(&(mqc->send_timer));
      }
      mqc->stat = DISCONNECTED;
    }
    else if (ev == receive_timeout_event){
      //if last receive has expired we need to stop and disconnect
      mqtt_sn_send_disconnect(mqc);
    }
    else if (ev == send_timeout_event){
      //if last send has expired, we need to send a pingreq
      mqtt_sn_send_pingreq(mqc);
    }
  }

  PROCESS_END();
}

#if 1
static void send_packet(struct mqtt_sn_connection *mqc, char* data, size_t len)
{
  simple_udp_send(&(mqc->sock), data, len);//these datatypes should all cast fine
  if (mqc->keep_alive>0 && mqc->stat == CONNECTED)
  {
    //normally we would use this to make sure that we are always sending data to keep the connection alive
    //but since the gateway does not support pubacks, we will always be relying on a
    //steady stream of pings to ensure that the connection stays alive.
    //ctimer_restart(&(mqc->send_timer));
  }
}
#endif
#if 0
static void* recieve_packet(int sock)
{
    static uint8_t buffer[MQTT_SN_MAX_PACKET_LENGTH+1];
    int length;
    int bytes_read;

    if (debug)
        fprintf(stderr, "waiting for packet...\n");

    // Read in the packet
    bytes_read = recv(sock, buffer, MQTT_SN_MAX_PACKET_LENGTH, 0);
    if (bytes_read < 0) {
        if (errno == EAGAIN) {
            if (debug)
                fprintf(stderr, "Timed out waiting for packet.\n");
            return NULL;
        } else {
            perror("recv failed");
            exit(EXIT_FAILURE);
        }
    }

    if (debug)
        fprintf(stderr, "Received %d bytes. Type=%s.\n", (int)bytes_read, mqtt_sn_type_string(buffer[1]));

    length = buffer[0];
    if (length == 0x01) {
        fprintf(stderr, "Error: packet received is longer than this tool can handle\n");
        exit(EXIT_FAILURE);
    }

    if (length != bytes_read) {
        fprintf(stderr, "Warning: read %d bytes but packet length is %d bytes.\n", (int)bytes_read, length);
    }

    // NULL-terminate the packet
    buffer[length] = '\0';

    // Store the last time that we received a packet
    last_receive = time(NULL);

    return buffer;
}
#endif
#if 1
void mqtt_sn_send_connect(struct mqtt_sn_connection *mqc, const char* client_id, uint16_t keepalive)
{
    connect_packet_t packet;

    // Check that it isn't too long
    if (client_id && strlen(client_id) > 23) {
        printf("Error: client id is too long\n");
        exit(EXIT_FAILURE);
    }

    // Create the CONNECT packet
    packet.type = MQTT_SN_TYPE_CONNECT;
    packet.flags = MQTT_SN_FLAG_CLEAN;
    packet.protocol_id = MQTT_SN_PROTOCOL_ID;
    packet.duration = uip_htons(keepalive);

    strncpy(packet.client_id, client_id, sizeof(packet.client_id)-1);
    packet.client_id[sizeof(packet.client_id) - 1] = '\0';

    packet.length = 0x06 + strlen(packet.client_id);

    if (debug)
        printf("Sending CONNECT packet...\n");

    // Store the keep alive period
    if (keepalive) {
        mqc->keep_alive = keepalive*CLOCK_SECOND;
    }
    mqc->stat = WAITING_ACK;

    return send_packet(mqc, (char*)&packet, packet.length);
}
#endif
#if 1
void mqtt_sn_send_register(struct mqtt_sn_connection *mqc, const char* topic_name)
{
    register_packet_t packet;
    size_t topic_name_len = strlen(topic_name);

    if (topic_name_len > MQTT_SN_MAX_TOPIC_LENGTH) {
        printf("Error: topic name is too long\n");
        exit(EXIT_FAILURE);
    }

    packet.type = MQTT_SN_TYPE_REGISTER;
    packet.topic_id = 0;
    packet.message_id = uip_htons(mqc->next_message_id++);
    strncpy(packet.topic_name, topic_name, sizeof(packet.topic_name));
    packet.length = 0x06 + strlen(packet.topic_name);

    if (debug)
        printf("Sending REGISTER packet...\n");

    return send_packet(mqc, (char*)&packet, packet.length);
}
#endif

#if 0
void mqtt_sn_send_regack(int sock, int topic_id, int mesage_id)
{
    regack_packet_t packet;
    packet.type = MQTT_SN_TYPE_REGACK;
    packet.topic_id = htons(topic_id);
    packet.message_id = htons(mesage_id);
    packet.return_code = 0x00;
    packet.length = 0x07;

    if (debug)
        fprintf(stderr, "Sending REGACK packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}
#endif

static uint8_t mqtt_sn_get_qos_flag(int8_t qos)
{
    switch (qos) {
        case -1:
          return MQTT_SN_FLAG_QOS_N1;
        case 0:
          return MQTT_SN_FLAG_QOS_0;
        case 1:
          return MQTT_SN_FLAG_QOS_1;
        case 2:
          return MQTT_SN_FLAG_QOS_2;
        default:
          return 0;
    }
}
#if 1
void mqtt_sn_send_publish(struct mqtt_sn_connection *mqc, uint16_t topic_id, uint8_t topic_type, const char* data, int8_t qos, uint8_t retain)
{
    publish_packet_t packet;
//this needs changed so that data is not assumed to be a null terminated string
    size_t data_len = strlen(data);

    if (data_len > sizeof(packet.data)) {
        printf("Error: payload is too big\n");
        exit(EXIT_FAILURE);
    }

    packet.type = MQTT_SN_TYPE_PUBLISH;
    packet.flags = 0x00;
    if (retain)
        packet.flags += MQTT_SN_FLAG_RETAIN;
    packet.flags += mqtt_sn_get_qos_flag(qos);
    packet.flags += (topic_type & 0x3);
    packet.topic_id = uip_htons(topic_id);
    packet.message_id = uip_htons(mqc->next_message_id++);
    strncpy(packet.data, data, sizeof(packet.data));
    packet.length = 0x07 + data_len;

    if (debug)
        printf("Sending PUBLISH packet...\n");

    return send_packet(mqc, (char*)&packet, packet.length);
}
#endif
#if 0
void mqtt_sn_send_subscribe(int sock, const char* topic_name, uint8_t qos)
{
    subscribe_packet_t packet;
    size_t topic_name_len = strlen(topic_name);

    packet.type = MQTT_SN_TYPE_SUBSCRIBE;
    packet.flags = 0x00;
    packet.flags += mqtt_sn_get_qos_flag(qos);
    if (topic_name_len == 2) {
        packet.flags += MQTT_SN_TOPIC_TYPE_SHORT;
    } else {
        packet.flags += MQTT_SN_TOPIC_TYPE_NORMAL;
    }
    packet.message_id = htons(next_message_id++);
    strncpy(packet.topic_name, topic_name, sizeof(packet.topic_name));
    packet.topic_name[sizeof(packet.topic_name)-1] = '\0';
    packet.length = 0x05 + topic_name_len;

    if (debug)
        fprintf(stderr, "Sending SUBSCRIBE packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);

}

void mqtt_sn_send_subscribe_topic_id(int sock, uint16_t topic_id, uint8_t qos)
{
    subscribe_packet_t packet;
    packet.type = MQTT_SN_TYPE_SUBSCRIBE;
    packet.flags = 0x00;
    packet.flags += mqtt_sn_get_qos_flag(qos);
    packet.flags += MQTT_SN_TOPIC_TYPE_PREDEFINED;
    packet.message_id = htons(next_message_id++);
    packet.topic_id = htons(topic_id);
    packet.length = 0x05 + 2;

    if (debug)
        fprintf(stderr, "Sending SUBSCRIBE packet...\n");

    return send_packet(sock, (char*)&packet, packet.length);
}

void mqtt_sn_send_pingreq(struct mqtt_sn_connection *mqc)
{
    char packet[2];

    packet[0] = 2;
    packet[1] = MQTT_SN_TYPE_PINGREQ;

    if (debug) {
        printf("Sending ping...\n");
    }

    ctimer_restart(&(mqc->send_timer));


    return send_packet(mqc, (char*)&packet, 2);
}
#if 1
void mqtt_sn_send_pingresp(struct mqtt_sn_connection *mqc)
{
    char packet[2];

    packet[0] = 2;
    packet[1] = MQTT_SN_TYPE_PINGRESP;

    if (debug) {
        printf("Sending ping response...\n");
    }

    return send_packet(mqc, (char*)&packet, 2);
}
#endif
#if 1
void mqtt_sn_send_disconnect(struct mqtt_sn_connection *mqc)
{
    disconnect_packet_t packet;
    packet.type = MQTT_SN_TYPE_DISCONNECT;
    packet.length = 0x02;

    if (debug)
        printf("Sending DISCONNECT packet...\n");

    send_packet(mqc, (char*)&packet, packet.length);
    process_post(&mqtt_sn_process, disconnect_event, mqc);
}
#endif
#if 0
void mqtt_sn_recieve_connack(int sock)
{
    connack_packet_t *packet = recieve_packet(sock);

    if (packet == NULL) {
        fprintf(stderr, "Failed to connect to MQTT-S gateway.\n");
        exit(EXIT_FAILURE);
    }

    if (packet->type != MQTT_SN_TYPE_CONNACK) {
        fprintf(stderr, "Was expecting CONNACK packet but received: 0x%2.2x\n", packet->type);
        exit(EXIT_FAILURE);
    }

    // Check Connack return code
    if (debug)
        fprintf(stderr, "CONNACK return code: 0x%2.2x\n", packet->return_code);

    if (packet->return_code) {
        fprintf(stderr, "CONNECT error: %s\n", mqtt_sn_return_code_string(packet->return_code));
        exit(packet->return_code);
    }
}
#endif
#if 0
static int mqtt_sn_process_register(int sock, const register_packet_t *packet)
{
    int message_id = ntohs(packet->message_id);
    int topic_id = ntohs(packet->topic_id);
    const char* topic_name = packet->topic_name;

    // Add it to the topic map
    mqtt_sn_register_topic(topic_id, topic_name);

    // Respond to gateway with REGACK
    mqtt_sn_send_regack(sock, topic_id, message_id);

    return 0;
}

void mqtt_sn_register_topic(int topic_id, const char* topic_name)
{
    topic_map_t **ptr = &topic_map;

    // Check topic ID is valid
    if (topic_id == 0x0000 || topic_id == 0xFFFF) {
        fprintf(stderr, "Error: attempted to register invalid topic id: 0x%4.4x\n", topic_id);
        return;
    }

    // Check topic name is valid
    if (topic_name == NULL || strlen(topic_name) < 0) {
        fprintf(stderr, "Error: attempted to register invalid topic name.\n");
        return;
    }

    if (debug)
        fprintf(stderr, "Registering topic 0x%4.4x: %s\n", topic_id, topic_name);

    // Look for the topic id
    while (*ptr) {
        if ((*ptr)->topic_id == topic_id) {
            break;
        } else {
            ptr = &((*ptr)->next);
        }
    }

    // Allocate memory for a new entry, if we reached the end of the list
    if (*ptr == NULL) {
        *ptr = (topic_map_t *)malloc(sizeof(topic_map_t));
        if (!*ptr) {
            fprintf(stderr, "Error: Failed to allocate memory for new topic map entry.\n");
            exit(EXIT_FAILURE);
        }
        (*ptr)->next = NULL;
    }

    // Copy in the name to the entry
    strncpy((*ptr)->topic_name, topic_name, MQTT_SN_MAX_TOPIC_LENGTH);
    (*ptr)->topic_id = topic_id;
}

const char* mqtt_sn_lookup_topic(int topic_id)
{
    topic_map_t **ptr = &topic_map;

    while (*ptr) {
        if ((*ptr)->topic_id == topic_id) {
            return (*ptr)->topic_name;
        }
        ptr = &((*ptr)->next);
    }

    fprintf(stderr, "Warning: failed to lookup topic id: 0x%4.4x\n", topic_id);
    return NULL;
}

uint16_t mqtt_sn_recieve_regack(int sock)
{
    regack_packet_t *packet = recieve_packet(sock);
    uint16_t received_message_id, received_topic_id;

    if (packet == NULL) {
        fprintf(stderr, "Failed to connect to register topic.\n");
        exit(EXIT_FAILURE);
    }

    if (packet->type != MQTT_SN_TYPE_REGACK) {
        fprintf(stderr, "Was expecting REGACK packet but received: 0x%2.2x\n", packet->type);
        exit(-1);
    }

    // Check Regack return code
    if (debug)
        fprintf(stderr, "REGACK return code: 0x%2.2x\n", packet->return_code);

    if (packet->return_code) {
        fprintf(stderr, "REGISTER error: %s\n", mqtt_sn_return_code_string(packet->return_code));
        exit(packet->return_code);
    }

    // Check that the Message ID matches
    received_message_id = ntohs( packet->message_id );
    if (received_message_id != next_message_id-1) {
        fprintf(stderr, "Warning: message id in Regack does not equal message id sent\n");
    }

    // Return the topic ID returned by the gateway
    received_topic_id = ntohs( packet->topic_id );
    if (debug)
        fprintf(stderr, "Topic ID: %d\n", received_topic_id);

    return received_topic_id;
}
#endif
#if 0
uint16_t mqtt_sn_recieve_suback(int sock)
{
    suback_packet_t *packet = recieve_packet(sock);
    uint16_t received_message_id, received_topic_id;

    if (packet == NULL) {
        fprintf(stderr, "Failed to subscribe to topic.\n");
        exit(EXIT_FAILURE);
    }

    if (packet->type != MQTT_SN_TYPE_SUBACK) {
        fprintf(stderr, "Was expecting SUBACK packet but received: 0x%2.2x\n", packet->type);
        exit(-1);
    }

    // Check Suback return code
    if (debug)
        fprintf(stderr, "SUBACK return code: 0x%2.2x\n", packet->return_code);

    if (packet->return_code) {
        fprintf(stderr, "SUBSCRIBE error: %s\n", mqtt_sn_return_code_string(packet->return_code));
        exit(packet->return_code);
    }

    // Check that the Message ID matches
    received_message_id = ntohs( packet->message_id );
    if (received_message_id != next_message_id-1) {
        fprintf(stderr, "Warning: message id in SUBACK does not equal message id sent\n");
        if (debug) {
            fprintf(stderr, "  Expecting: %d\n", next_message_id-1);
            fprintf(stderr, "  Actual: %d\n", received_message_id);
        }
    }

    // Return the topic ID returned by the gateway
    received_topic_id = ntohs( packet->topic_id );
    if (debug)
        fprintf(stderr, "Topic ID: %d\n", received_topic_id);

    return received_topic_id;
}
#endif
#if 0
publish_packet_t* mqtt_sn_loop(int sock, int timeout)
{
    time_t now = time(NULL);
    struct timeval tv;
    fd_set rfd;
    int ret;

    // Time to send a ping?
    if (keep_alive > 0 && (now - last_transmit) >= keep_alive) {
        mqtt_sn_send_pingreq(sock);
    }

    FD_ZERO(&rfd);
    FD_SET(sock, &rfd);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    ret = select(FD_SETSIZE, &rfd, NULL, NULL, &tv);
    if (ret < 0) {
        if (errno != EINTR) {
            // Something is wrong.
            perror("select");
            exit(EXIT_FAILURE);
        }
    } else if (ret > 0) {
        char* packet;

        // Receive a packet
        packet = recieve_packet(sock);
        if (packet) {
            switch(packet[1]) {
                case MQTT_SN_TYPE_PUBLISH: {
                    return (publish_packet_t*)packet;
                    break;
                }

                case MQTT_SN_TYPE_REGISTER: {
                    mqtt_sn_process_register(sock, (register_packet_t*)packet);
                    break;
                };

                case MQTT_SN_TYPE_PINGRESP: {
                    // do nothing
                    break;
                };

                case MQTT_SN_TYPE_DISCONNECT: {
                    fprintf(stderr, "Warning: Received DISCONNECT from gateway.\n");
                    exit(EXIT_FAILURE);
                    break;
                };

                default: {
                    const char* type = mqtt_sn_type_string(packet[1]);
                    fprintf(stderr, "Warning: unexpected packet type: %s.\n", type);
                    break;
                }
            }
        }
    }

    // Check for receive timeout
    if (keep_alive > 0 && (now - last_receive) >= (keep_alive * 1.5)) {
        fprintf(stderr, "Keep alive error: timed out receiving packet from gateway.\n");
        exit(EXIT_FAILURE);
    }

    return NULL;
}
#endif
#if 1
const char* mqtt_sn_type_string(uint8_t type)
{
    switch(type) {
        case MQTT_SN_TYPE_ADVERTISE:     return "ADVERTISE";
        case MQTT_SN_TYPE_SEARCHGW:      return "SEARCHGW";
        case MQTT_SN_TYPE_GWINFO:        return "GWINFO";
        case MQTT_SN_TYPE_CONNECT:       return "CONNECT";
        case MQTT_SN_TYPE_CONNACK:       return "CONNACK";
        case MQTT_SN_TYPE_WILLTOPICREQ:  return "WILLTOPICREQ";
        case MQTT_SN_TYPE_WILLTOPIC:     return "WILLTOPIC";
        case MQTT_SN_TYPE_WILLMSGREQ:    return "WILLMSGREQ";
        case MQTT_SN_TYPE_WILLMSG:       return "WILLMSG";
        case MQTT_SN_TYPE_REGISTER:      return "REGISTER";
        case MQTT_SN_TYPE_REGACK:        return "REGACK";
        case MQTT_SN_TYPE_PUBLISH:       return "PUBLISH";
        case MQTT_SN_TYPE_PUBACK:        return "PUBACK";
        case MQTT_SN_TYPE_PUBCOMP:       return "PUBCOMP";
        case MQTT_SN_TYPE_PUBREC:        return "PUBREC";
        case MQTT_SN_TYPE_PUBREL:        return "PUBREL";
        case MQTT_SN_TYPE_SUBSCRIBE:     return "SUBSCRIBE";
        case MQTT_SN_TYPE_SUBACK:        return "SUBACK";
        case MQTT_SN_TYPE_UNSUBSCRIBE:   return "UNSUBSCRIBE";
        case MQTT_SN_TYPE_UNSUBACK:      return "UNSUBACK";
        case MQTT_SN_TYPE_PINGREQ:       return "PINGREQ";
        case MQTT_SN_TYPE_PINGRESP:      return "PINGRESP";
        case MQTT_SN_TYPE_DISCONNECT:    return "DISCONNECT";
        case MQTT_SN_TYPE_WILLTOPICUPD:  return "WILLTOPICUPD";
        case MQTT_SN_TYPE_WILLTOPICRESP: return "WILLTOPICRESP";
        case MQTT_SN_TYPE_WILLMSGUPD:    return "WILLMSGUPD";
        case MQTT_SN_TYPE_WILLMSGRESP:   return "WILLMSGRESP";
        default:                       return "UNKNOWN";
    }
}
#endif
#if 1
const char* mqtt_sn_return_code_string(uint8_t return_code)
{
    switch(return_code) {
        case 0x00: return "Accepted";
        case 0x01: return "Rejected: congestion";
        case 0x02: return "Rejected: invalid topic ID";
        case 0x03: return "Rejected: not supported";
        default:   return "Rejected: unknown reason";
    }
}
#endif
#if 0
void mqtt_sn_cleanup()
{
    topic_map_t **ptr = &topic_map;
    topic_map_t **ptr2 = NULL;

    // Walk through the topic map, deleting each entry
    while (*ptr) {
        ptr2 = ptr;
        ptr = &((*ptr)->next);
        free(*ptr2);
        *ptr2 = NULL;
    }
}
#endif

