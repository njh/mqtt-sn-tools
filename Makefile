all: contiki-mqtt-sn-pub
PROJECT_SOURCEFILES += mqtts.c

WITH_UIP6=1
UIP_CONF_IPV6=1
CFLAGS+= -DUIP_CONF_IPV6_RPL
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

CONTIKI=/home/user/contiki
include $(CONTIKI)/Makefile.include
