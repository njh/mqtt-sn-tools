
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

////when streaming, the number of frames that will be sent per second
//#define STREAM_FRAMES_PER_SECOND 2
////the number of 16 bit samples in each frame
//#define ECG_FRAME_CAPACITY 100
//#define PPG_FRAME_CAPACITY 15
//#define RESP_FRAME_CAPACITY 15
////Note: the desire frames per second should be the product of frame capacity and frames per second
//
//#define  VP_LIST_SIZE 20
//#define  VP_MSG_RCD_CNT 9

//turn off TCP in order to reduce ROM/RAM
#define UIP_CONF_TCP 0

////Ports for UDP
//#define UDP_PORT 5688
//#define UDP_PORT2 5689
////Channels if using Rime
//#define VITALUCAST_CHANNEL 225
//#define VITALUCAST_CHANNEL2 226
//
////ripplecomm messages are identified by a dispatch byte value and a version in the header
//#define RIPPLECOMM_DISPATCH 0xD2
//
//#define RIPPLECOMM_VERSION 0
//#define RIPPLECOMM_VERSION_COMPATIBLE(x) (RIPPLECOMM_VERSION == x)
//
//#ifdef UIP_CONF_BUFFER_SIZE
//#undef UIP_CONF_BUFFER_SIZE
//#endif
//#define UIP_CONF_BUFFER_SIZE    350
//
//#ifndef UIP_CONF_RECEIVE_WINDOW
//#define UIP_CONF_RECEIVE_WINDOW  200
//#endif

#ifdef NETSTACK_CONF_RDC
#undef NETSTACK_CONF_RDC
#endif
#ifdef NETSTACK_CONF_MAC
#undef NETSTACK_CONF_MAC
#endif

#define NETSTACK_CONF_MAC nullmac_driver
#define NETSTACK_CONF_RDC nullrdc_driver


//#define NODE_TYPE_COLLECTOR 1

//#define NETSTACK_CONF_NETWORK rime_driver
//#define NETSTACK_CONF_MAC     csma_driver
//#define NETSTACK_CONF_MAC     nullmac_driver
//#define NETSTACK_CONF_RDC     sicslowmac_driver
//#define NETSTACK_CONF_RDC     nullrdc_driver
//#define NETSTACK_CONF_RADIO   contiki_maca_driver
//#define NETSTACK_CONF_RADIO   cc2420_driver
//#define NETSTACK_CONF_FRAMER  framer_nullmac
#endif /* PROJECT_CONF_H_ */
