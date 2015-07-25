$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnPubTest < Minitest::Test

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-pub', '-?')
    assert_match /^Usage: mqtt-sn-pub/, @cmd_result[0]
  end

  def test_custom_client_id
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Connect) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-i' => 'test_custom_client_id',
          '-T' => 10,
          '-m' => 'message',
          '-p' => fs.port
        )
      end
    end

    assert_empty @cmd_result
    assert_equal 'test_custom_client_id', @packet.client_id
    assert_equal 30, @packet.keep_alive
  end

  def test_publish_qos_n1
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-q' => -1,
          '-T' => 10,
          '-m' => 'test_publish_qos_n1',
          '-p' => fs.port
        )
      end
    end

    assert_empty @cmd_result
    assert_equal 10, @packet.topic_id
    assert_equal :predefined, @packet.topic_id_type
    assert_equal 'test_publish_qos_n1', @packet.data
    assert_equal -1, @packet.qos
    assert_equal false, @packet.retain
  end

  def test_publish_debug
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-d',
          '-t', 'topic',
          '-m', 'test_publish_qos_0_debug',
          '-p', fs.port]
        )
      end
    end

    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Sending CONNECT packet/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG waiting for packet/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG CONNACK return code: 0x00/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Sending PUBLISH packet/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Sending DISCONNECT packet/, @cmd_result
  end

  def test_publish_qos_0
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-q' => 0,
          '-t' => 'topic',
          '-m' => 'test_publish_qos_0',
          '-p' => fs.port
        )
      end
    end

    assert_empty @cmd_result
    assert_equal 1, @packet.topic_id
    assert_equal :normal, @packet.topic_id_type
    assert_equal 'test_publish_qos_0', @packet.data
    assert_equal 0, @packet.qos
    assert_equal false, @packet.retain
  end

  def test_publish_qos_0_short
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-q' => 0,
          '-t' => 'TT',
          '-m' => 'test_publish_qos_0_short',
          '-p' => fs.port
        )
      end
    end

    assert_empty @cmd_result
    assert_equal 'TT', @packet.topic_id
    assert_equal :short, @packet.topic_id_type
    assert_equal 'test_publish_qos_0_short', @packet.data
    assert_equal 0, @packet.qos
    assert_equal false, @packet.retain
  end

  def test_publish_qos_0_predefined
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-q' => 0,
          '-T' => 127,
          '-m' => 'test_publish_qos_0_predefined',
          '-p' => fs.port
        )
      end
    end

    assert_empty @cmd_result
    assert_equal 127, @packet.topic_id
    assert_equal :predefined, @packet.topic_id_type
    assert_equal 'test_publish_qos_0_predefined', @packet.data
    assert_equal 0, @packet.qos
    assert_equal false, @packet.retain
  end

  def test_publish_qos_0_retained
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-r',
          '-t', 'topic',
          '-m', 'test_publish_retained',
          '-p', fs.port]
        )
      end
    end

    assert_empty @cmd_result
    assert_equal 1, @packet.topic_id
    assert_equal :normal, @packet.topic_id_type
    assert_equal 'test_publish_retained', @packet.data
    assert_equal 0, @packet.qos
    assert_equal true, @packet.retain
  end

  def test_publish_qos_0_empty
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-r', '-n',
          '-t', 'topic',
          '-p', fs.port]
        )
      end
    end

    assert_empty @cmd_result
    assert_equal 1, @packet.topic_id
    assert_equal :normal, @packet.topic_id_type
    assert_equal '', @packet.data
    assert_equal 0, @packet.qos
    assert_equal true, @packet.retain
  end

end
