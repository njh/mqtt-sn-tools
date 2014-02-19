#!/bin/bash

#start RSMB
#gnome-terminal -t RSMB -x /home/arenner/mqtt/rsmb_git/org.eclipse.mosquitto.rsmb/rsmb/src/broker_mqtts rsmb_test.conf

#start mosquitto
#LD_LIBRARY_PATH="/home/arenner/mqtt/mosquitto-1.2.1/lib/"
##export LD_LIBRARY_PATH
#gnome-terminal -t mosquitto -x /home/arenner/mqtt/mosquitto-1.2.1/src/mosquitto
#start suscriber for the test topic
LD_LIBRARY_PATH="/home/arenner/mqtt/mosquitto-1.2.1/lib/"
export LD_LIBRARY_PATH
gnome-terminal -t subscriber -x /home/arenner/mqtt/mosquitto-1.2.1/client/mosquitto_sub -h 127.0.0.1 -t AR
#gnome-terminal -t subscriber -x /home/arenner/mqtt/mosquitto-1.2.1/client/mosquitto_sub -h 172.16.220.128 -t AR




#use host mosquitto until openssl is configured

#start mqtts gateway - connect to mosquitto on vmnet1 for now
#gnome-terminal -t gateway -x em-mqtts-gateway -D -A 192.168.212.1
 
#start suscriber for the test topic
