MQTT-SN Tools
=============

[![Build Status](https://travis-ci.org/njh/mqtt-sn-tools.svg?branch=master)](https://travis-ci.org/njh/mqtt-sn-tools)

Command line tools written in C for the MQTT-SN (MQTT for Sensor Networks) protocol.


Supported Features
------------------

- QoS 0, 1 and -1
- Keep alive pings
- Publishing retained messages
- Publishing empty messages
- Subscribing to named topic
- Clean / unclean sessions
- Manual and automatic client ID generation
- Displaying topic name with wildcard subscriptions
- Pre-defined topic IDs and short topic names
- Forwarder encapsulation according to MQTT-SN Protocol Specification v1.2.


Limitations
-----------

- Packets must be 255 or less bytes long
- No Last Will and Testament
- No QoS 2
- No automatic re-sending of lost packets
- No Automatic gateway discovery


Building
--------

Just run 'make' on a POSIX system.


Publishing
----------

    Usage: mqtt-sn-pub [opts] -t <topic> -m <message>

      -d             Increase debug level by one. -d can occur multiple times.
      -f <file>      A file to send as the message payload.
      -h <host>      MQTT-SN host to connect to. Defaults to '127.0.0.1'.
      -i <clientid>  ID to use for this client. Defaults to 'mqtt-sn-tools-' with process id.
      -k <keepalive> keep alive in seconds for this client. Defaults to 10.
      -m <message>   Message payload to send.
      -l             Read from STDIN, one message per line.
      -n             Send a null (zero length) message.
      -p <port>      Network port to connect to. Defaults to 1883.
      -q <qos>       Quality of Service value (0, 1 or -1). Defaults to 0.
      -r             Message should be retained.
      -s             Read one whole message from STDIN.
      -t <topic>     MQTT-SN topic name to publish to.
      -T <topicid>   Pre-defined MQTT-SN topic ID to publish to.
      --fe           Enables Forwarder Encapsulation. Mqtt-sn packets are encapsulated according to MQTT-SN Protocol Specification v1.2, chapter 5.5 Forwarder Encapsulation.
      --wlnid        If Forwarder Encapsulation is enabled, wireless node ID for this client. Defaults to process id.


Subscribing
-----------

    Usage: mqtt-sn-sub [opts] -t <topic>

      -1             exit after receiving a single message.
      -c             disable 'clean session' (store subscription and pending messages when client disconnects).
      -d             Increase debug level by one. -d can occur multiple times.
      -h <host>      MQTT-SN host to connect to. Defaults to '127.0.0.1'.
      -i <clientid>  ID to use for this client. Defaults to 'mqtt-sn-tools-' with process id.
      -k <keepalive> keep alive in seconds for this client. Defaults to 10.
      -p <port>      Network port to connect to. Defaults to 1883.
      -q <qos>       QoS level to subscribe with (0 or 1). Defaults to 0.
      -t <topic>     MQTT-SN topic name to subscribe to. It may repeat multiple times.
      -T <topicid>   Pre-defined MQTT-SN topic ID to subscribe to. It may repeat multiple times.
      --fe           Enables Forwarder Encapsulation. Mqtt-sn packets are encapsulated according to MQTT-SN Protocol Specification v1.2, chapter 5.5 Forwarder Encapsulation.
      --wlnid        If Forwarder Encapsulation is enabled, wireless node ID for this client. Defaults to process id.
      -v             Print messages verbosely, showing the topic name.
      -V             Print messages verbosely, showing current time and the topic name.


Dumping
-------

Displays MQTT-SN packets sent to specified port.
Most useful for listening out for QoS -1 messages being published by a client.

    Usage: mqtt-sn-dump [opts] -p <port>

      -a             Dump all packet types.
      -d             Increase debug level by one. -d can occur multiple times.
      -p <port>      Network port to listen on. Defaults to 1883.
      -v             Print messages verbosely, showing the topic name.


Serial Port Bridge
------------------

The Serial Port bridge can be used to relay packets from a remote device on the end of a
serial port and convert them into UDP packets, which are sent and received from a broker
or MQTT-SN gateway.

    Usage: mqtt-sn-serial-bridge [opts] <device>

      -b <baud>      Set the baud rate. Defaults to 9600.
      -d             Increase debug level by one. -d can occur multiple times.
      -dd            Enable extended debugging - display packets in hex.
      -h <host>      MQTT-SN host to connect to. Defaults to '127.0.0.1'.
      -p <port>      Network port to connect to. Defaults to 1883.
      --fe           Enables Forwarder Encapsulation. Mqtt-sn packets are encapsulated according to MQTT-SN Protocol Specification v1.2, chapter 5.5 Forwarder Encapsulation.


License
-------

MQTT-SN Tools is licensed under the [MIT License].



[MIT License]: http://opensource.org/licenses/MIT
