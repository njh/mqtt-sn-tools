#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
//#include "net/uip-debug.h"
#include "mqtt-sn.h"

#include "node-id.h"

#include "simple-udp.h"
//#include "servreg-hack.h"

#include <stdio.h>
#include <string.h>

#define UDP_PORT 1884
#define SERVICE_ID 190

#define SEND_INTERVAL		(10 * CLOCK_SECOND)
#define SEND_TIME		(random_rand() % (SEND_INTERVAL))

static struct mqtt_sn_connection mqtt_sn_c;
static char *mqtt_client_id="sensor";
static char topic_name[]="AR";
static uint16_t mqtt_keep_alive=20;
const char *message_data = NULL;
static uint16_t topic_id = 0;
static uint8_t topic_id_type = MQTT_SN_TOPIC_TYPE_SHORT;
static int8_t qos = 0;
uint8_t retain = FALSE;
//uint8_t debug = FALSE;

enum connection_status
{
  REQUESTED = 0,
  REJECTED_CONGESTION,
  REQUEST_TIMEOUT,
  ACKNOWLEDGED,

};



/*---------------------------------------------------------------------------*/
PROCESS(unicast_sender_process, "Unicast sender example process");
AUTOSTART_PROCESSES(&unicast_sender_process);
/*---------------------------------------------------------------------------*/
static void
puback_receiver(struct mqtt_sn_connection *mqc, const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  printf("Puback received\n");
}
/*---------------------------------------------------------------------------*/
static void
connack_receiver(struct mqtt_sn_connection *mqc, const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  printf("Connack received\n");
}
/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;
  int i;
  uint8_t state;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  printf("IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      //uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
    }
  }
}

/*---------------------------------------------------------------------------*/
/*Add callbacks here if we make them*/
static const struct mqtt_sn_callbacks mqtt_sn_call = {NULL,NULL,connack_receiver,NULL,puback_receiver,NULL};

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_sender_process, ev, data)
{
  static struct etimer periodic_timer;
  static struct etimer send_timer;
  static uip_ipaddr_t addr;

  PROCESS_BEGIN();

  //servreg_hack_init();

  set_global_address();
  //send to TUN interface for cooja simulation
  //uip_ip6addr(&addr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
  //uip_ip6addr(&addr, 0x2001, 0x0db8, 1, 0xffff, 0, 0, 0xc0a8, 0xd480);//192.168.212.128 with tayga
  //uip_ip6addr(&addr, 0xaaaa, 0, 2, 0xeeee, 0, 0, 0xc0a8, 0xd480);//192.168.212.128 with tayga
  uip_ip6addr(&addr, 0xaaaa, 0, 2, 0xeeee, 0, 0, 0xac10, 0xdc01);//172.16.220.1 with tayga
  mqtt_sn_create_socket(&mqtt_sn_c,35555, &addr, UDP_PORT);
  (&mqtt_sn_c)->mc = &mqtt_sn_call;


  //connect, presume ack comes before timer fires
  //mqtt_sn_send_connect(&unicast_connection,mqtt_client_id,mqtt_keep_alive);

  etimer_set(&periodic_timer, 20*CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    printf("requesting connection \n ");
    mqtt_sn_send_connect(&mqtt_sn_c,mqtt_client_id,mqtt_keep_alive);
    //etimer_reset(&periodic_timer);
    etimer_set(&send_timer, SEND_TIME);
  while(1) {


    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));
    printf("publishing \n ");
    //addr = servreg_hack_lookup(SERVICE_ID);
    //if(addr != NULL) {
      static unsigned int message_number;
      char buf[20];

//      printf("Sending unicast to ");
//      uip_debug_ipaddr_print(&addr);
//      printf("\n");
      sprintf(buf, "Message %d", message_number);
      message_number++;

      topic_id = (topic_name[0] << 8) + topic_name[1];

      mqtt_sn_send_publish(&mqtt_sn_c, topic_id,topic_id_type,buf,qos,retain);
      etimer_reset(&send_timer);
      //simple_udp_sendto(&unicast_connection, buf, strlen(buf) + 1, addr);
    //} else {
     // printf("Service %d not found\n", SERVICE_ID);
   // }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

