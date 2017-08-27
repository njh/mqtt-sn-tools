$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnDumpTest < Minitest::Test

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-dump', '-?')
    assert_match(/^Usage: mqtt-sn-dump/, @cmd_result[0])
  end

  def publish_packet(port, packet)
    # FIXME: better way to wait until socket is open?
    sleep 0.2
    socket = UDPSocket.new
    socket.connect('localhost', port)
    socket << packet.to_s
    socket.close
  end

  def publish_qos_n1_packet(port)
    publish_packet(port,
      MQTT::SN::Packet::Publish.new(
        :topic_id => 'TT',
        :topic_id_type => :short,
        :data => "Message for TT",
        :qos => -1
      )
    )
  end

  def test_receive_qos_n1
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-p', @port]
    ) do |cmd|
      publish_qos_n1_packet(@port)
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["Message for TT"], @cmd_result)
  end

  def test_receive_qos_n1_debug
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-d', '-p', @port]
    ) do |cmd|
      publish_qos_n1_packet(@port)
      wait_for_output_then_kill(cmd)
    end

    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG mqtt-sn-dump listening on port \d+/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG waiting for packet/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Received 21 bytes from 127.0.0.1\:\d+. Type=PUBLISH/, @cmd_result)
  end

  def test_receive_qos_n1_verbose
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-v', '-p', @port]
    ) do |cmd|
      publish_qos_n1_packet(@port)
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["TT: Message for TT"], @cmd_result)
  end

  def test_receive_qos_n1_dump_all
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_qos_n1_packet(@port)
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["PUBLISH: len=21 topic_id=0x5454 message_id=0x0000 data=Message for TT"], @cmd_result)
  end

  def test_receive_qos_n1_term
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-p', @port]
    ) do |cmd|
      publish_qos_n1_packet(@port)
      wait_for_output_then_kill(cmd, 'TERM')
    end

    assert_equal(["Message for TT"], @cmd_result)
  end

  def test_receive_qos_n1_hup
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-p', @port]
    ) do |cmd|
      publish_qos_n1_packet(@port)
      wait_for_output_then_kill(cmd, 'HUP')
    end

    assert_equal(["Message for TT"], @cmd_result)
  end

  def test_receive_connect
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port,
        MQTT::SN::Packet::Connect.new(
          :client_id => 'my_client_id',
          :keep_alive => 10
        )
      )
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["CONNECT: len=18 protocol_id=1 duration=10 client_id=my_client_id"], @cmd_result)
  end

  def test_receive_connack
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port,
        MQTT::SN::Packet::Connack.new(
          :return_code => 1
        )
      )
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["CONNACK: len=3 return_code=1 (Rejected: congestion)"], @cmd_result)
  end

  def test_receive_connack_not_supported
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port,
        MQTT::SN::Packet::Connack.new(
          :return_code => 3
        )
      )
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["CONNACK: len=3 return_code=3 (Rejected: not supported)"], @cmd_result)
  end

  def test_receive_register
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port,
        MQTT::SN::Packet::Register.new(
          :id => 10,
          :topic_id => 20,
          :topic_name => 'Topic Name'
        )
      )
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["REGISTER: len=16 topic_id=0x0014 message_id=0x000a topic_name=Topic Name"], @cmd_result)
  end

  def test_receive_regack
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port,
        MQTT::SN::Packet::Regack.new(
          :id => 30,
          :topic_id => 40,
          :return_code => 0
        )
      )
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["REGACK: len=7 topic_id=0x0028 message_id=0x001e return_code=0 (Accepted)"], @cmd_result)
  end

  def test_receive_subscribe
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port,
        MQTT::SN::Packet::Subscribe.new(:id => 50)
      )
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["SUBSCRIBE: len=5 message_id=0x0032"], @cmd_result)
  end

  def test_receive_suback
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port,
        MQTT::SN::Packet::Suback.new(
          :id => 60,
          :topic_id => 70,
          :return_code => 0
        )
      )
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["SUBACK: len=8 topic_id=0x0046 message_id=0x003c return_code=0 (Accepted)"], @cmd_result)
  end

  def test_receive_suback_unknown_error
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port,
        MQTT::SN::Packet::Suback.new(
          :id => 60,
          :topic_id => 70,
          :return_code => 5
        )
      )
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["SUBACK: len=8 topic_id=0x0046 message_id=0x003c return_code=5 (Rejected: unknown reason)"], @cmd_result)
  end

  def test_receive_pingreq
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port, MQTT::SN::Packet::Pingreq.new)
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["PINGREQ: len=2"], @cmd_result)
  end

  def test_receive_pingresp
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port, MQTT::SN::Packet::Pingresp.new)
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["PINGRESP: len=2"], @cmd_result)
  end

  def test_receive_disconnect
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port, MQTT::SN::Packet::Disconnect.new)
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["DISCONNECT: len=2 duration=0"], @cmd_result)
  end

  def test_receive_unknown
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port, "\x03\xFF\x00")
      wait_for_output_then_kill(cmd)
    end

    assert_equal(["UNKNOWN: len=3"], @cmd_result)
  end

  def test_receive_invalid_length
    @port = random_port
    @cmd_result = run_cmd(
      'mqtt-sn-dump',
      ['-a', '-p', @port]
    ) do |cmd|
      publish_packet(@port, "\x00\x00\x00\x00")
      wait_for_output_then_kill(cmd)
    end

    assert_match(/WARN  Packet length header is not valid/, @cmd_result[0])
  end

end
