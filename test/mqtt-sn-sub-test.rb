$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnSubTest < Minitest::Test

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-sub', '-?')
    assert_match(/^Usage: mqtt-sn-sub/, @cmd_result[0])
  end

  def test_no_arguments
    @cmd_result = run_cmd('mqtt-sn-sub')
    assert_match(/^Usage: mqtt-sn-sub/, @cmd_result[0])
  end

  def test_default_client_id
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Connect) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1',
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal(["Message for test"], @cmd_result)
    assert_match(/^mqtt-sn-tools-(\d+)$/, @packet.client_id)
    assert_equal(10, @packet.keep_alive)
    assert_equal(true, @packet.clean_session)
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

    assert_equal(["Message for test"], @cmd_result)
    assert_equal('test_custom_client_id', @packet.client_id)
    assert_equal(10, @packet.keep_alive)
    assert_equal(true, @packet.clean_session)
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

    assert_equal(["Message for test"], @cmd_result)
    assert_match(/^mqtt-sn-tools/, @packet.client_id)
    assert_equal(5, @packet.keep_alive)
    assert_equal(true, @packet.clean_session)
  end

  def test_connack_timeout
    fake_server do |fs|
      def fs.handle_connect(packet)
        nil
      end

      fs.wait_for_packet(MQTT::SN::Packet::Connect) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1',
          '-d',
          '-k', 2,
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_includes_match(/Timed out waiting for packet/, @cmd_result)
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

    assert_equal(["Message for test"], @cmd_result)
    assert_equal('test', @packet.topic_name)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal(0, @packet.qos)
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

    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Debug level is: 1/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Sending CONNECT packet/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG waiting for packet/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG CONNACK return code: 0x00/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Sending SUBSCRIBE packet/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG SUBACK return code: 0x00/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Sending DISCONNECT packet/, @cmd_result)
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

    assert_equal(["test: Message for test"], @cmd_result)
    assert_equal('test', @packet.topic_name)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal(0, @packet.qos)
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

    assert_equal(1, @cmd_result.count)
    assert_match(/\d{4}\-\d{2}\-\d{2} \d{2}\:\d{2}\:\d{2} test: Message for test/, @cmd_result[0])
    assert_equal('test', @packet.topic_name)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal(0, @packet.qos)
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

    assert_equal(["tt: Message for tt"], @cmd_result)
    assert_equal('tt', @packet.topic_name)
    assert_equal(:short, @packet.topic_id_type)
    assert_equal(0, @packet.qos)
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

    assert_equal(["0011: Message for #17"], @cmd_result)
    assert_nil(@packet.topic_name)
    assert_equal(17, @packet.topic_id)
    assert_equal(:predefined, @packet.topic_id_type)
    assert_equal(0, @packet.qos)
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

    assert_equal(["test: Message for test"], @cmd_result)
    assert_equal('test', @packet.topic_name)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal(0, @packet.qos)
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

    assert_equal(["test: Message for test"], @cmd_result)
    assert_equal('test', @packet.topic_name)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal(0, @packet.qos)
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

    assert_equal(["test: Message for test"], @cmd_result)
    assert_equal('test', @packet.topic_name)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal(0, @packet.qos)
  end

  def test_subscribe_two_topic_names
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v',
        '-q', 1,
        '-t', 'test1',
        '-t', 'test2',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Puback)
        wait_for_output_then_kill(cmd, 'INT')
      end
    end

    assert_equal(2, @cmd_result.length)
    assert_equal("test1: Message for test1", @cmd_result[0])
    assert_equal("test2: Message for test2", @cmd_result[1])
  end

  def test_subscribe_multiple_topics
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v',
        '-q', 1,
        '-t', 'name',
        '-t', 'TT',
        '-T', 0x10,
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Puback)
        wait_for_output_then_kill(cmd, 'INT')
      end
    end

    assert_equal(3, @cmd_result.length)
    assert_equal("name: Message for name", @cmd_result[0])
    assert_equal("TT: Message for TT", @cmd_result[1])
    assert_equal("0010: Message for #16", @cmd_result[2])
  end

  def test_subscribe_thirty_topics
    topics = (1..30).map { |t| ['-t', "topic#{t}"] }

    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-v',
        '-q', 1,
        topics,
        '-p', fs.port,
        '-h', fs.address].compact
      ) do |cmd|
        @packet = fs.wait_for_packet(MQTT::SN::Packet::Puback)
        wait_for_output_then_kill(cmd, 'INT')
      end
    end

    assert_equal(30, @cmd_result.length)
    assert_equal("topic1: Message for topic1", @cmd_result[0])
    assert_equal("topic30: Message for topic30", @cmd_result[29])
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

    assert_includes_match(/ERROR SUBSCRIBE error: Rejected: invalid topic ID/, @cmd_result)
  end

  def test_subscribe_one_qos1
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Puback) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1',
          '-q', 1,
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal(["Message for test"], @cmd_result)
    assert_equal(1, @packet.topic_id)
    assert_equal(0, @packet.id)
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

    assert_includes_match(/Packet length header is not valid/, @cmd_result)
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

    assert_includes_match(/Read 3 bytes but packet length is 4 bytes/, @cmd_result)
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

    assert_includes_match(/WARN  Message id in SUBACK does not equal message id sent/, @cmd_result)
    assert_includes_match(/DEBUG   Expecting: 1/, @cmd_result)
    assert_includes_match(/DEBUG   Actual: 222/, @cmd_result)
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

    assert_includes_match(/DEBUG Debug level is: 1/, @cmd_result)
    assert_includes_match(/DEBUG Got interrupt signal/, @cmd_result)
    assert_includes_match(/^test: Message for test$/, @cmd_result)
    assert_equal('test', @packet.topic_name)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal(0, @packet.qos)
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

    assert_equal(["old_topic: old_msg", "test: Message for test"], @cmd_result)
    assert_match(/^mqtt-sn-tools/, @packet.client_id)
    assert_equal(10, @packet.keep_alive)
    assert_equal(false, @packet.clean_session)
  end

  def test_register_invalid_topic_id
    @fs = fake_server do |fs|
      def fs.handle_connect(packet)
        [
          MQTT::SN::Packet::Connack.new(:return_code => 0x00),
          MQTT::SN::Packet::Register.new(:topic_id => 0, :topic_name => 'old_topic')
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

    assert_match(/ERROR Attempted to register invalid topic id: 0x0000/, @cmd_result[0])
    assert_match(/test: Message for test/, @cmd_result[1])
  end

  def test_register_invalid_topic_name
    @fs = fake_server do |fs|
      def fs.handle_connect(packet)
        [
          MQTT::SN::Packet::Connack.new(:return_code => 0x00),
          MQTT::SN::Packet::Register.new(:topic_id => 5, :topic_name => ''),
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

    assert_match(/ERROR Attempted to register invalid topic name/, @cmd_result[0])
    assert_match(/WARN  Failed to lookup topic id: 0x0005/, @cmd_result[1])
    assert_match(/test: Message for test/, @cmd_result[2])
  end

  def test_recieve_non_registered_topic_id
    @fs = fake_server do |fs|
      def fs.handle_connect(packet)
        [
          MQTT::SN::Packet::Connack.new(:return_code => 0x00),
          MQTT::SN::Packet::Publish.new(:topic_id => 5, :data => 'not registered')
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

    assert_match(/Failed to lookup topic id: 0x0005/, @cmd_result[0])
    assert_match(/test: Message for test/, @cmd_result[1])
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

    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Received 265 bytes from/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ WARN  Packet received is longer than this tool can handle/, @cmd_result)
  end

  def test_disconnect_after_recieve
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Disconnect) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1',
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal(["Message for test"], @cmd_result)
    assert_kind_of(MQTT::SN::Packet::Disconnect, @packet)
    assert_nil(@packet.duration)
  end

  def test_disconnect_after_recieve_with_sleep
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Disconnect) do
        @cmd_result = run_cmd(
          'mqtt-sn-sub',
          ['-1',
          '-e', 3600,
          '-t', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_equal(["Message for test"], @cmd_result)
    assert_equal(MQTT::SN::Packet::Disconnect, @packet.class)
    assert_equal(3600, @packet.duration)
  end
end
