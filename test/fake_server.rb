#!/usr/bin/env ruby
#
# This is a 'fake' MQTT-SN server to help with testing client implementations
#
# It behaves in the following ways:
#   * Responds to CONNECT with a successful CONACK
#   * Responds to PUBLISH by keeping a copy of the packet
#   * Responds to SUBSCRIBE with SUBACK and a PUBLISH to the topic
#   * Responds to PINGREQ with PINGRESP and keeps a count
#   * Responds to DISCONNECT with a DISCONNECT packet
#
# It has the following restrictions
#   * Doesn't deal with timeouts
#   * Only handles a single client
#

require 'logger'
require 'socket'
require 'mqtt'


class MQTT::SN::FakeServer
  attr_reader :address, :port
  attr_reader :thread
  attr_reader :last_connect
  attr_reader :last_publish
  attr_reader :pings_received
  attr_accessor :logger

  # Create a new fake MQTT server
  #
  # If no port is given, bind to a random port number
  # If no bind address is given, bind to localhost
  def initialize(port=0, bind_address='127.0.0.1')
    @port = port
    @address = bind_address
    @pings_received = 0
  end

  # Get the logger used by the server
  def logger
    @logger ||= Logger.new(STDOUT)
  end

  # Start the thread and open the socket that will process client connections
  def start
    @thread ||= Thread.new do
      Socket.udp_server_sockets(@address, @port) do |sockets|
        @address = sockets.first.local_address.ip_address
        @port = sockets.first.local_address.ip_port
        logger.info "Started a fake MQTT-SN server on #{@address}:#{@port}"
        Socket.udp_server_loop_on(sockets) do |data, client|
          response = process_packet(data)
          unless response.nil?
            response = [response] unless response.kind_of?(Enumerable)
            response.each do |packet|
              logger.debug "Replying with: #{packet.inspect}"
              client.reply(packet.to_s)
            end
          end
        end
      end
    end
  end

  # Stop the thread and close the socket
  def stop
    logger.info "Stopping fake MQTT-SN server"
    @thread.kill if @thread and @thread.alive?
    @thread = nil
  end

  # Start the server thread and wait for it to finish (possibly never)
  def run
    start
    begin
      @thread.join
    rescue Interrupt
      stop
    end
  end

  def wait_for_port_number
    while @port.to_i == 0
      Thread.pass
    end
    @port
  end

  def wait_for_connect(timeout=2)
    @last_connect = nil
    yield
    Timeout.timeout(timeout) do
      Thread.pass while @last_connect.nil?
    end
    @last_connect
  end
  
  def wait_for_publish(timeout=2)
    @last_publish = nil
    yield
    Timeout.timeout(timeout) do
      Thread.pass while @last_publish.nil?
    end
    @last_publish
  end

  protected

  def process_packet(data)
    packet = MQTT::SN::Packet.parse(data)
    logger.debug "Received: #{packet.inspect}"

    case packet
      when MQTT::SN::Packet::Connect
        @last_connect = packet
        MQTT::SN::Packet::Connack.new(:return_code => 0)
      when MQTT::SN::Packet::Publish
        @last_publish = packet
        nil
      when MQTT::SN::Packet::Pingreq
        @pings_received += 1
        MQTT::SN::Packet::Pingresp.new
      when MQTT::SN::Packet::Subscribe
        [ 
          MQTT::SN::Packet::Suback.new(:id => packet.id, :topic_id => 1, :return_code => 0),
          MQTT::SN::Packet::Publish.new(:topic_id => 1, :data => 'Hello World')
        ]
      when MQTT::SN::Packet::Disconnect
        MQTT::SN::Packet::Disconnect.new
      when MQTT::SN::Packet::Register
        MQTT::SN::Packet::Regack.new(:id => packet.id, :topic_id => 1, :return_code => 0)
      else
        logger.warn "Unhandled packet type: #{packet.class}"
        nil
    end

    rescue MQTT::SN::ProtocolException => e
      logger.warn "Protocol error: #{e}"
      nil
  end
end

if __FILE__ == $0
  server = MQTT::SN::FakeServer.new(MQTT::SN::DEFAULT_PORT)
  server.logger.level = Logger::DEBUG
  server.run
end
