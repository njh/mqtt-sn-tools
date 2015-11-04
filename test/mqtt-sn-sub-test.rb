$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnSubTest < Minitest::Test

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-sub', '-?')
    assert_match /^Usage: mqtt-sn-sub/, @cmd_result[0]
  end

  def test_no_arguments
    @cmd_result = run_cmd('mqtt-sn-sub')
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

    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Debug level is: 1/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Sending CONNECT packet/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG waiting for packet/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG CONNACK return code: 0x00/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Sending SUBSCRIBE packet/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG SUBACK return code: 0x00/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Sending DISCONNECT packet/, @cmd_result
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

  def test_subscribe_then_int
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v',
        '-t', 'test',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe)
        wait_for_output_then_kill(cmd, 'INT')
      end
    end

    assert_equal ["test: Hello World"], @cmd_result
    assert_equal 'test', @packet.topic_name
    assert_equal :normal, @packet.topic_id_type
    assert_equal 0, @packet.qos
  end

  def test_subscribe_then_term
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v',
        '-t', 'test',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe)
        wait_for_output_then_kill(cmd, 'TERM')
      end
    end

    assert_equal ["test: Hello World"], @cmd_result
    assert_equal 'test', @packet.topic_name
    assert_equal :normal, @packet.topic_id_type
    assert_equal 0, @packet.qos
  end

  def test_subscribe_then_hup
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v',
        '-t', 'test',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe)
        wait_for_output_then_kill(cmd, 'HUP')
      end
    end

    assert_equal ["test: Hello World"], @cmd_result
    assert_equal 'test', @packet.topic_name
    assert_equal :normal, @packet.topic_id_type
    assert_equal 0, @packet.qos
  end

  def test_subscribe_invalid_topic_id
    fake_server do |fs|
      def fs.handle_subscribe(packet)
        MQTT::SN::Packet::Suback.new(
          :id => packet.id,
          :topic_id => 0,
          :topic_id_type => packet.topic_id_type,
          :return_code => 0x02
        )
      end

      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v',
        '-T', 123,
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe)
        wait_for_output_then_kill(cmd, 'HUP')
      end
    end

    assert_includes_match /ERROR SUBSCRIBE error: Rejected: invalid topic ID/, @cmd_result
  end

  def test_subscribe_invalid_connack_packet
    fake_server do |fs|
      def fs.handle_connect(packet)
        "\x00\x05\x00"
      end

      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v',
        '-T', 123,
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Connect)
        wait_for_output_then_kill(cmd)
      end
    end

    assert_includes_match /Packet length header is not valid/, @cmd_result
  end

  def test_subscribe_incorrect_connack_packet_length
    fake_server do |fs|
      def fs.handle_connect(packet)
        "\x04\x05\x00"
      end

      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v',
        '-T', 123,
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Connect)
        wait_for_output_then_kill(cmd)
      end
    end

    assert_includes_match /Read 3 bytes but packet length is 4 bytes/, @cmd_result
  end

  def test_subscribe_id_mismatch
    fake_server do |fs|
      def fs.handle_subscribe(packet)
        MQTT::SN::Packet::Suback.new(
          :id => 222,
          :topic_id => 0,
          :topic_id_type => packet.topic_id_type,
          :return_code => 0x00
        )
      end

      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v', '-d',
        '-t', 'test',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe)
        wait_for_output_then_kill(cmd)
      end
    end

    assert_includes_match /WARN  Message id in SUBACK does not equal message id sent/, @cmd_result
    assert_includes_match /DEBUG   Expecting: 1/, @cmd_result
    assert_includes_match /DEBUG   Actual: 222/, @cmd_result
  end

  def test_subscribe_then_interupt_debug
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v', '-d',
        '-t', 'test',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe)
        wait_for_output_then_kill(cmd)
      end
    end

    assert_includes_match /DEBUG Debug level is: 1/, @cmd_result
    assert_includes_match /DEBUG Got interrupt signal/, @cmd_result
    assert_includes_match /^test: Hello World$/, @cmd_result
    assert_equal 'test', @packet.topic_name
    assert_equal :normal, @packet.topic_id_type
    assert_equal 0, @packet.qos
  end

  def test_subscribe_no_clean_session
    @fs = fake_server do |fs|
      def fs.handle_connect(packet)
        [
          MQTT::SN::Packet::Connack.new(:return_code => 0x00),
          MQTT::SN::Packet::Register.new(:topic_id => 5, :topic_name => 'old_topic'),
          MQTT::SN::Packet::Publish.new(:topic_id => 5, :data => 'old_msg')
        ]
      end

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

  def test_packet_too_long
    fake_server do |fs|
      def fs.handle_subscribe(packet)
        super(packet, 'x' * 256)
      end

      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v', '-d',
        '-t', 'test',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Subscribe)
        wait_for_output_then_kill(cmd)
      end
    end

    assert_includes_match /[\d\-]+ [\d\:]+ DEBUG Received 265 bytes from/, @cmd_result
    assert_includes_match /[\d\-]+ [\d\:]+ WARN  Packet received is longer than this tool can handle/, @cmd_result
  end

  def test_both_topic_name_and_id
    @cmd_result = run_cmd(
      'mqtt-sn-sub',
      '-t' => 'topic_name',
      '-T' => 10,
    )
    assert_match /Please provide either a topic id or a topic name, not both/, @cmd_result[0]
  end

end
