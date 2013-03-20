Command line tools written in C for the MQTT-S (MQTT For Sensor Networks) protocol.


Building
--------

Just run 'make'.


Publishing
----------

    Usage: mqtts-pub [opts] -t <topic> -m <message>

      -d             Enable debug messages.
      -h <host>      MQTT-S host to connect to. Defaults to '127.0.0.1'.
      -i <clientid>  ID to use for this client. Defaults to 'mqtts-client-984'.
      -m <message>   Message payload to send.
      -n             Send a null (zero length) message.
      -p <port>      Network port to connect to. Defaults to 1883.
      -r             Message should be retained.
      -t <topic>     MQTT topic name to publish to.
