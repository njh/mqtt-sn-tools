/*
  Basic MQTT-SN client library
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
#include <stdarg.h>

#ifndef MQTT_SN_H
#define MQTT_SN_H

#ifndef FALSE
#define FALSE  (0)
#endif

#ifndef TRUE
#define TRUE  (1)
#endif


#define MQTT_SN_MAX_PACKET_LENGTH  (255)
#define MQTT_SN_MAX_TOPIC_LENGTH   (MQTT_SN_MAX_PACKET_LENGTH-6)
#define MQTT_SN_MAX_WIRELESS_NODE_ID_LENGTH  (252)

#define MQTT_SN_TYPE_ADVERTISE     (0x00)
#define MQTT_SN_TYPE_SEARCHGW      (0x01)
#define MQTT_SN_TYPE_GWINFO        (0x02)
#define MQTT_SN_TYPE_CONNECT       (0x04)
#define MQTT_SN_TYPE_CONNACK       (0x05)
#define MQTT_SN_TYPE_WILLTOPICREQ  (0x06)
#define MQTT_SN_TYPE_WILLTOPIC     (0x07)
#define MQTT_SN_TYPE_WILLMSGREQ    (0x08)
#define MQTT_SN_TYPE_WILLMSG       (0x09)
#define MQTT_SN_TYPE_REGISTER      (0x0A)
#define MQTT_SN_TYPE_REGACK        (0x0B)
#define MQTT_SN_TYPE_PUBLISH       (0x0C)
#define MQTT_SN_TYPE_PUBACK        (0x0D)
#define MQTT_SN_TYPE_PUBCOMP       (0x0E)
#define MQTT_SN_TYPE_PUBREC        (0x0F)
#define MQTT_SN_TYPE_PUBREL        (0x10)
#define MQTT_SN_TYPE_SUBSCRIBE     (0x12)
#define MQTT_SN_TYPE_SUBACK        (0x13)
#define MQTT_SN_TYPE_UNSUBSCRIBE   (0x14)
#define MQTT_SN_TYPE_UNSUBACK      (0x15)
#define MQTT_SN_TYPE_PINGREQ       (0x16)
#define MQTT_SN_TYPE_PINGRESP      (0x17)
#define MQTT_SN_TYPE_DISCONNECT    (0x18)
#define MQTT_SN_TYPE_WILLTOPICUPD  (0x1A)
#define MQTT_SN_TYPE_WILLTOPICRESP (0x1B)
#define MQTT_SN_TYPE_WILLMSGUPD    (0x1C)
#define MQTT_SN_TYPE_WILLMSGRESP   (0x1D)
#define MQTT_SN_TYPE_FRWDENCAP     (0xFE)

#define MQTT_SN_TOPIC_TYPE_NORMAL     (0x00)
#define MQTT_SN_TOPIC_TYPE_PREDEFINED (0x01)
#define MQTT_SN_TOPIC_TYPE_SHORT      (0x02)


#define MQTT_SN_FLAG_DUP     (0x1 << 7)
#define MQTT_SN_FLAG_QOS_0   (0x0 << 5)
#define MQTT_SN_FLAG_QOS_1   (0x1 << 5)
#define MQTT_SN_FLAG_QOS_2   (0x2 << 5)
#define MQTT_SN_FLAG_QOS_N1  (0x3 << 5)
#define MQTT_SN_FLAG_RETAIN  (0x1 << 4)
#define MQTT_SN_FLAG_WILL    (0x1 << 3)
#define MQTT_SN_FLAG_CLEAN   (0x1 << 2)

#define MQTT_SN_PROTOCOL_ID  (0x01)

typedef struct {
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  uint8_t protocol_id;
  uint16_t duration;
  char client_id[23];
} connect_packet_t;

typedef struct {
  uint8_t length;
  uint8_t type;
  uint8_t return_code;
} connack_packet_t;

typedef struct {
  uint8_t length;
  uint8_t type;
  uint16_t topic_id;
  uint16_t message_id;
  char topic_name[MQTT_SN_MAX_TOPIC_LENGTH];
} register_packet_t;

typedef struct {
  uint8_t length;
  uint8_t type;
  uint16_t topic_id;
  uint16_t message_id;
  uint8_t return_code;
} regack_packet_t;

typedef struct __attribute__((packed)) {
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  uint16_t topic_id;
  uint16_t message_id;
  char data[MQTT_SN_MAX_PACKET_LENGTH-7];
} publish_packet_t;

typedef struct __attribute__((packed)) {
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  uint16_t message_id;
  union {
      char topic_name[MQTT_SN_MAX_TOPIC_LENGTH];
      uint16_t topic_id;
  };
} subscribe_packet_t;

typedef struct __attribute__((packed)) {
  uint8_t length;
  uint8_t type;
  uint8_t flags;
  uint16_t topic_id;
  uint16_t message_id;
  uint8_t return_code;
} suback_packet_t;

typedef struct {
  uint8_t length;
  uint8_t type;
  uint16_t duration;
} disconnect_packet_t;

typedef struct __attribute__((packed)) {
  uint8_t length;
  uint8_t type;
  uint8_t ctrl;
  uint8_t wireless_node_id[MQTT_SN_MAX_WIRELESS_NODE_ID_LENGTH];
  char data[MQTT_SN_MAX_PACKET_LENGTH];
} frwdencap_packet_t;

typedef struct topic_map {
  uint16_t topic_id;
  char topic_name[MQTT_SN_MAX_TOPIC_LENGTH];
  struct topic_map *next;
} topic_map_t;


// Library functions
int mqtt_sn_create_socket(const char* host, const char* port);
void mqtt_sn_send_connect(int sock, const char* client_id, uint16_t keepalive);
void mqtt_sn_send_register(int sock, const char* topic_name);
void mqtt_sn_send_publish(int sock, uint16_t topic_id, uint8_t topic_type, const void* data, int8_t qos, uint8_t retain);
void mqtt_sn_send_subscribe_topic_name(int sock, const char* topic_name, uint8_t qos);
void mqtt_sn_send_subscribe_topic_id(int sock, uint16_t topic_id, uint8_t qos);
void mqtt_sn_send_pingreq(int sock);
void mqtt_sn_send_disconnect(int sock);
void mqtt_sn_receive_disconnect(int sock);
void mqtt_sn_receive_connack(int sock);
uint16_t mqtt_sn_receive_regack(int sock);
uint16_t mqtt_sn_receive_suback(int sock);
publish_packet_t* mqtt_sn_loop(int sock, int timeout);
void mqtt_sn_register_topic(int topic_id, const char* topic_name);
const char* mqtt_sn_lookup_topic(int topic_id);
void mqtt_sn_cleanup();

void mqtt_sn_set_debug(uint8_t value);
const char* mqtt_sn_type_string(uint8_t type);
const char* mqtt_sn_return_code_string(uint8_t return_code);

uint8_t mqtt_sn_validate_packet(const void *packet, size_t length);
void mqtt_sn_send_packet(int sock, void* data);
void mqtt_sn_send_frwdencap_packet(int sock, void* data, const uint8_t *wireless_node_id , uint8_t wireless_node_id_len );
void* mqtt_sn_receive_packet(int sock);
void* mqtt_sn_receive_frwdencap_packet(int sock, uint8_t **wireless_node_id , uint8_t *wireless_node_id_len);

// Functions to turn on and off forwarder encapsulation according to MQTT-SN Protocol Specification v1.2,
// chapter 5.5 Forwarder Encapsulation.
uint8_t mqtt_sn_enable_frwdencap();
uint8_t mqtt_sn_disable_frwdencap();
// Set wireless node ID and wireless node ID length
void mqtt_sn_set_frwdencap_parameters( const uint8_t *wlnid, uint8_t wlnid_len ) ;
// Wrap mqtt-sn packet into a forwarder encapsulation packet
frwdencap_packet_t* mqtt_sn_create_frwdencap_packet( const uint8_t *data , size_t *len , const uint8_t *wireless_node_id , uint8_t wireless_node_id_len ) ;
// Print content of a buffer in hexadecimal format into a stream. It is used to log wireless node ID.
void fprint_wlnid( FILE * stream , const uint8_t *wireless_node_id , uint8_t wireless_node_id_len ) ;

void log_debug(const char * format, ... );
void log_warn(const char * format, ... );
void log_err(const char * format, ... );

#endif
