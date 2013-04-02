Command line tools written in C for the MQTT-S (MQTT For Sensor Networks) protocol.

Supported Features
------------------

- QoS 0
- Keep alive pings
- Publishing retained messages
- Publishing empty messages
- Subscribing to named topic
- Clean / unclean sessions
- Manual and automatic client ID generation


Unsupported Features
--------------------

- QoS -1, 1 and 2
- Last Will and Testament
- Displaying topic name with each recieved message (aka verbose mode)
- Packets longer than 255 bytes
- Automatic gateway discovery
- Pre-defined topic IDs and short topic names


Building
--------

Just run 'make' on a POSIX system.


Publishing
----------

    Usage: mqtts-pub [opts] -t <topic> -m <message>

      -d             Enable debug messages.
      -h <host>      MQTT-S host to connect to. Defaults to '127.0.0.1'.
      -i <clientid>  ID to use for this client. Defaults to mqtts-client- appended with the process id'.
      -m <message>   Message payload to send.
      -n             Send a null (zero length) message.
      -p <port>      Network port to connect to. Defaults to 1883.
      -r             Message should be retained.
      -t <topic>     MQTT topic name to publish to.


Subscribing
-----------

    Usage: mqtts-sub [opts] -t <topic>

      -1             exit after receiving a single message.
      -c             disable 'clean session' (store subscription and pending messages when client disconnects).
      -d             Enable debug messages.
      -h <host>      MQTT-S host to connect to. Defaults to '127.0.0.1'.
      -i <clientid>  ID to use for this client. Defaults to mqtts-client- appended with the process id'.
      -k <keepalive> keep alive in seconds for this client. Defaults to 0.
      -p <port>      Network port to connect to. Defaults to 1883.
      -t <topic>     MQTT topic name to subscribe to.
