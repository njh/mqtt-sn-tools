Release History
===============

Version 0.0.6 (2017-08-23)
--------------------------

- Added support for publishing and subscribing with QoS 1
- Added support for reading from a file using -f
- Added support for reading from STDIN using -s
- Added support for publishing one message per line using -l
- Added support for subscribing to multiple topics
- Added timeout when waiting for packets
- Made Keep Alive configurable in mqtt-sn-pub
- Changed default keep alive for mqtt-sn-pub to 10 seconds
- Fix for displaying IPv6 addresses
- Improved debug error messages when connecting fails


Version 0.0.5 (2015-11-04)
--------------------------

- Added extensive test suite written in Ruby
- Added return code strings to packet dump output
- Display when connecting and disconnecting starts.
- Changed error messages that aren't fatal to warnings
- Added signal handling to mqtt-sn-dump
- Fix to ensure that packet data structures are set to 0 before use
- Added mqtt-sn-dump tool
- Added debug of source IP and port when receiving a packet
- Implemented setting the Clean Session flag
- Fix for subscribing to short/predefined topics
- New command line option -V to display current time and the topic name in verbose mode.
- Added install and uninstall targets to the Makefile
- Added Forwarder Encapsulation support
- Added current time and log level to log messages
- Wait for disconnect acknowledge into mqtt-sn-pub and mqtt-sn-sub.
- Display string, not hex for unexpected packet type
- Fixed incorrect pointer handling in mqtt-sn-sub


Version 0.0.4 (2014-06-01)
--------------------------

- Changed license to MIT
- Added mqtt-sn-serial-bridge
- Simplified the Makefile
- Improvements to cope with multiple packets received in close succession


Version 0.0.3 (2013-12-07)
--------------------------

- Added BSD 2-Clause License
- Renamed 'Unsupported Features' to limitations.
- Added checks to make sure various strings aren’t too long
- Corrected the maximum length of a MQTT-SN packet (in this tool)
- Added support for displaying short and pre-defined topic types in verbose mode
- Added support for pre-defined Topic IDs in the subscribing client (note that this is untested because I don’t have a broker that supports it)
- Added support for displaying topic name for wildcard subscriptions (aka verbose mode)
- Noted that QoS −1 is now supported in the README
- Added support for QoS Level −1 (tested against RSMB)
- Corrected help text for pre-defined topic ID parameter
- Shortened clientid argument help text
- Enabled keep-alive timer in mqtt-sn-pub - will cause sessions to be cleaned up by the broker if client is unexpectedly disconnected


Version 0.0.2 (2013-12-02)
--------------------------

- Renamed from MQTT-S to MQTT-SN to match specification change
- Added support for publishing to short and pre-defined topic ids
- Added setting of QoS flag in matts_send_publish
- Renamed default client id prefix to mqtts-tools-


Version 0.0.1 (2013-04-02)
--------------------------

- Initial Release
- Contains just mqtts-sub and mqtts-pub
