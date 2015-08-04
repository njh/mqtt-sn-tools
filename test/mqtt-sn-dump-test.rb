$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnDumpTest < Minitest::Test

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-dump', '-?')
    assert_match /^Usage: mqtt-sn-dump/, @cmd_result[0]
  end

  def publish_qos_n1_packet(port)
    socket = UDPSocket.new
    socket.connect('localhost', port)
    socket << MQTT::SN::Packet::Publish.new(
      :topic_id => 'TT',
      :topic_id_type => :short,
      :data => "Hello World",
      :qos => -1
    )
    socket.close
  end

  def test_receive_qos_n1
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-p', @port]
    ) do |cmd|
      # FIXME: better way to wait until socket is open?
      sleep 0.2
      publish_qos_n1_packet(@port)
      wait_for_output_then_kill(cmd)
    end

    assert_equal ["Hello World"], @cmd_result
  end

  def test_receive_qos_n1_debug
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-d', '-p', @port]
    ) do |cmd|
      # FIXME: better way to wait until socket is open?
      sleep 0.2
      publish_qos_n1_packet(@port)
      wait_for_output_then_kill(cmd)
    end

    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG mqtt-sn-dump listening on port \d+/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG waiting for packet/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Received 18 bytes from 127.0.0.1\:\d+. Type=PUBLISH/, @cmd_result
  end

  def test_receive_qos_n1_verbose
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-v', '-p', @port]
    ) do |cmd|
      # FIXME: better way to wait until socket is open?
      sleep 0.2
      publish_qos_n1_packet(@port)
      wait_for_output_then_kill(cmd)
    end

    assert_equal ["TT: Hello World"], @cmd_result
  end

  def test_receive_qos_n1_dump_all
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      # FIXME: better way to wait until socket is open?
      sleep 0.2
      publish_qos_n1_packet(@port)
      wait_for_output_then_kill(cmd)
    end

    assert_equal ["PUBLISH: len=18 topic_id=0x5454 message_id=0x0000 data=Hello World"], @cmd_result
  end

end
