$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnSubTest < Minitest::Test

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-sub', '-?')
    assert_match /^Usage: mqtt-sn-sub/, @cmd_result[0]
  end

  def test_custom_client_id
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Connect) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1',
          '-i', 'test_custom_client_id',
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal ["Hello World"], @cmd_result
    assert_equal 'test_custom_client_id', @packet.client_id
    assert_equal 10, @packet.keep_alive
    assert_equal true, @packet.clean_session
  end

  def test_custom_keep_alive
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Connect) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1',
          '-k', 5,
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal ["Hello World"], @cmd_result
    assert_match /^mqtt-sn-tools/, @packet.client_id
    assert_equal 5, @packet.keep_alive
    assert_equal true, @packet.clean_session
  end

  def test_subscribe_one
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1',
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal ["Hello World"], @cmd_result
    assert_equal 'test', @packet.topic_name
    assert_equal :normal, @packet.topic_id_type
    assert_equal 0, @packet.qos
  end

  def test_subscribe_one_debug
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-d', '-1',
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Sending CONNECT packet/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG waiting for packet/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG CONNACK return code: 0x00/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Sending SUBSCRIBE packet/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG SUBACK return code: 0x00/, @cmd_result
  end

  def test_subscribe_one_verbose
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1', '-v',
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal ["test: Hello World"], @cmd_result
    assert_equal 'test', @packet.topic_name
    assert_equal :normal, @packet.topic_id_type
    assert_equal 0, @packet.qos
  end

  def test_subscribe_one_verbose_time
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1', '-V',
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal 1, @cmd_result.count
    assert_match /\d{4}\-\d{2}\-\d{2} \d{2}\:\d{2}\:\d{2} test: Hello World/, @cmd_result[0]
    assert_equal 'test', @packet.topic_name
    assert_equal :normal, @packet.topic_id_type
    assert_equal 0, @packet.qos
  end

  def test_subscribe_one_short
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1', '-v',
          '-t', 'tt',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal ["tt: Hello World"], @cmd_result
    assert_equal 'tt', @packet.topic_name
    assert_equal :short, @packet.topic_id_type
    assert_equal 0, @packet.qos
  end

  def test_subscribe_one_predefined
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1', '-v',
          '-T', 17,
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal ["0011: Hello World"], @cmd_result
    assert_nil @packet.topic_name
    assert_equal 17, @packet.topic_id
    assert_equal :predefined, @packet.topic_id_type
    assert_equal 0, @packet.qos
  end

  def test_subscribe_then_interupt
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v',
        '-t', 'test',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe)
        wait_for_output_then_kill(cmd)
      end
    end

    assert_equal ["test: Hello World"], @cmd_result
    assert_equal 'test', @packet.topic_name
    assert_equal :normal, @packet.topic_id_type
    assert_equal 0, @packet.qos
  end

  def test_subscribe_no_clean_session
    @fs = fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-c', '-v',
        '-t', 'test',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Connect)
        wait_for_output_then_kill(cmd)
      end
    end

    assert_equal ["old_topic: old_msg", "test: Hello World"], @cmd_result
    assert_match /^mqtt-sn-tools/, @packet.client_id
    assert_equal 10, @packet.keep_alive
    assert_equal false, @packet.clean_session
  end

end
