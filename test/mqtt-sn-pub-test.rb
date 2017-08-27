$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnPubTest < Minitest::Test

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-pub', '-?')
    assert_match(/^Usage: mqtt-sn-pub/, @cmd_result[0])
  end

  def test_no_arguments
    @cmd_result = run_cmd('mqtt-sn-pub')
    assert_match(/^Usage: mqtt-sn-pub/, @cmd_result[0])
  end

  def test_custom_client_id
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Connect) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-i' => 'test_custom_client_id',
          '-T' => 10,
          '-m' => 'message',
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal('test_custom_client_id', @packet.client_id)
    assert_equal(10, @packet.keep_alive)
    assert_equal(true, @packet.clean_session)
  end

  def test_23char_client_id
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Connect) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-i' => 'ABCDEFGHIJKLMNOPQRSTUVW',
          '-T' => 10,
          '-m' => 'message',
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal('ABCDEFGHIJKLMNOPQRSTUVW', @packet.client_id)
    assert_equal(10, @packet.keep_alive)
    assert_equal(true, @packet.clean_session)
  end

  def test_connack_congestion
    fake_server do |fs|
      def fs.handle_connect(packet)
        MQTT::SN::Packet::Connack.new(:return_code => 0x01)
      end

      fs.wait_for_packet(MQTT::SN::Packet::Connect) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-T' => 10,
          '-m' => 'message',
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_match(/CONNECT error: Rejected: congestion/, @cmd_result[0])
  end

  def test_no_connack
    fake_server do |fs|
      def fs.handle_connect(packet)
        MQTT::SN::Packet::Disconnect.new
      end

      fs.wait_for_packet(MQTT::SN::Packet::Connect) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-T' => 10,
          '-m' => 'message',
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_match(/ERROR Was expecting CONNACK packet but received: DISCONNECT/, @cmd_result[0])
  end

  def test_too_long_client_id
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-pub',
        '-i' => 'C' * 255,
        '-T' => 10,
        '-m' => 'message',
        '-p' => fs.port,
        '-h' => fs.address
      )
    end

    assert_match(/ERROR Client id is too long/, @cmd_result[0])
  end

  def test_publish_qos_n1
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-q' => -1,
          '-T' => 10,
          '-m' => 'test_publish_qos_n1',
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal(10, @packet.topic_id)
    assert_equal(:predefined, @packet.topic_id_type)
    assert_equal('test_publish_qos_n1', @packet.data)
    assert_equal(-1, @packet.qos)
    assert_equal(false, @packet.retain)
  end

  def test_publish_debug
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-d',
          '-t', 'topic',
          '-m', 'test_publish_qos_0_debug',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Debug level is: 1/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Sending CONNECT packet/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG waiting for packet/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG CONNACK return code: 0x00/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Sending PUBLISH packet/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Sending DISCONNECT packet/, @cmd_result)
  end

  def test_publish_debug_2
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-d', '-d',
          '-i', 'fixed_client_id',
          '-t', 'topic',
          '-m', 'test_publish_qos_0_debug',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Debug level is: 2/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Sending CONNECT packet/, @cmd_result)
    assert_includes_match(/Sending  21 bytes\. Type=CONNECT on Socket: 3/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG waiting for packet/, @cmd_result)
    assert_includes_match(/Received  3 bytes from 127.0.0.1\:\d+. Type=CONNACK on Socket/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG CONNACK return code: 0x00/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Sending PUBLISH packet/, @cmd_result)
    assert_includes_match(/[\d\-]+ [\d\:]+ DEBUG Sending DISCONNECT packet/, @cmd_result)
  end

  def test_publish_from_file
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-t', 'topic',
          '-f', 'test.txt',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal(1, @packet.topic_id)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal('The is the contents of test.txt', @packet.data)
    assert_equal(0, @packet.qos)
  end

  def test_publish_from_file
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-t', 'topic',
          '-f', 'test.txt',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal(1, @packet.topic_id)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal('The is the contents of test.txt', @packet.data)
    assert_equal(0, @packet.qos)
  end

  def test_publish_from_file_too_big
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-t', 'topic',
          '-f', 'test_big.txt',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_match(/WARN  Input file is longer than the maximum message size/, @cmd_result[0])

    assert_equal(1, @packet.topic_id)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal(248, @packet.data.length)
    assert_equal(0, @packet.qos)
  end

  def test_publish_from_file_hyphen
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-t', 'topic',
          '-f', '-',
          '-p', fs.port,
          '-h', fs.address],
          'Message from file -'
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal('Message from file -', @packet.data)
  end

  def test_publish_from_stdin
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-t', 'topic',
          '-s',
          '-p', fs.port,
          '-h', fs.address],
          'Message from STDIN'
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal('Message from STDIN', @packet.data)
  end

  def test_publish_multiline_from_stdin
    fs = fake_server do |fs|
      fs.wait_for_packet(MQTT::SN::Packet::Disconnect) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-t', 'topic',
          '-l',
          '-p', fs.port,
          '-h', fs.address],
          "Message 1\nMessage 2\nMessage 3\n"
        )
      end
    end

    publish_packets = fs.packets_received.select do |packet|
      packet.is_a?(MQTT::SN::Packet::Publish)
    end

    assert_empty(@cmd_result)
    assert_equal(['Message 1', 'Message 2', 'Message 3'], publish_packets.map {|p| p.data})
    assert_equal([1, 1, 1], publish_packets.map {|p| p.topic_id})
    assert_equal([:normal, :normal, :normal], publish_packets.map {|p| p.topic_id_type})
    assert_equal([0, 0, 0], publish_packets.map {|p| p.qos})
  end

  def test_publish_multiline_from_stdin_no_newline
    fs = fake_server do |fs|
      fs.wait_for_packet(MQTT::SN::Packet::Disconnect) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-t', 'topic',
          '-l',
          '-p', fs.port,
          '-h', fs.address],
          "Message 1\nMessage 2"
        )
      end
    end

    publish_packets = fs.packets_received.select do |packet|
      packet.is_a?(MQTT::SN::Packet::Publish)
    end

    assert_includes_match(/Failed to find newline when reading message/, @cmd_result)
    assert_equal(['Message 1'], publish_packets.map {|p| p.data})
    assert_equal([1], publish_packets.map {|p| p.topic_id})
    assert_equal([:normal], publish_packets.map {|p| p.topic_id_type})
    assert_equal([0], publish_packets.map {|p| p.qos})
  end

  def test_publish_qos_0
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-q' => 0,
          '-t' => 'topic',
          '-m' => 'test_publish_qos_0',
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal(1, @packet.topic_id)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal('test_publish_qos_0', @packet.data)
    assert_equal(0, @packet.qos)
    assert_equal(false, @packet.retain)
  end

  def test_publish_qos_0_short
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-q' => 0,
          '-t' => 'TT',
          '-m' => 'test_publish_qos_0_short',
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal('TT', @packet.topic_id)
    assert_equal(:short, @packet.topic_id_type)
    assert_equal('test_publish_qos_0_short', @packet.data)
    assert_equal(0, @packet.qos)
    assert_equal(false, @packet.retain)
  end

  def test_publish_qos_0_predefined
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-q' => 0,
          '-T' => 127,
          '-m' => 'test_publish_qos_0_predefined',
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal(127, @packet.topic_id)
    assert_equal(:predefined, @packet.topic_id_type)
    assert_equal('test_publish_qos_0_predefined', @packet.data)
    assert_equal(0, @packet.qos)
    assert_equal(false, @packet.retain)
  end

  def test_publish_qos_0_retained
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-r',
          '-t', 'topic',
          '-m', 'test_publish_retained',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal(1, @packet.topic_id)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal('test_publish_retained', @packet.data)
    assert_equal(0, @packet.qos)
    assert_equal(true, @packet.retain)
  end

  def test_publish_qos_0_empty
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-r', '-n',
          '-t', 'topic',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal(1, @packet.topic_id)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal('', @packet.data)
    assert_equal(0, @packet.qos)
    assert_equal(true, @packet.retain)
  end

  def test_publish_qos_1
    fake_server do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-q' => 1,
          '-t' => 'topic',
          '-m' => 'test_publish_qos_1',
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_empty(@cmd_result)
    assert_equal(1, @packet.topic_id)
    assert_equal(:normal, @packet.topic_id_type)
    assert_equal('test_publish_qos_1', @packet.data)
    assert_equal(1, @packet.qos)
    assert_equal(false, @packet.retain)
  end

  def test_publish_qos_1_puback_timeout
    fake_server do |fs|
      def fs.handle_publish(packet)
        nil
      end

      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          '-q' => 1,
          '-d' => '',
          '-k' => 2,
          '-t' => 'topic',
          '-m' => 'test_publish_qos_1',
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_includes_match(/Timed out while waiting for a PUBACK from gateway/, @cmd_result)
  end

  def test_publish_ipv6
    unless have_ipv6?
      skip("IPv6 is not available on this system")
    end

    fs = fake_server(nil, '::1') do |fs|
      @packet = fs.wait_for_packet(MQTT::SN::Packet::Publish) do
        @cmd_result = run_cmd(
          'mqtt-sn-pub',
          ['-d', '-d',
          '-t', 'topic',
          '-m', 'test',
          '-p', fs.port,
          '-h', fs.address]
        )
      end
    end

    assert_includes_match(/Received  3 bytes from ::1:#{fs.port}/, @cmd_result)
    assert_equal('test', @packet.data)
  end

  def test_invalid_qos
    @cmd_result = run_cmd(
      'mqtt-sn-pub',
      '-q' => '2',
      '-t' => 'topic',
      '-m' => 'message'
    )
    assert_match(/Only QoS level 0, 1 or -1 is supported/, @cmd_result[0])
  end

  def test_payload_too_big
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-pub',
        ['-t', 'topic',
        '-m', 'm' * 255,
        '-p', fs.port,
        '-h', fs.address]
      )
    end
    assert_match(/Payload is too big/, @cmd_result[0])
  end

  def test_both_topic_name_and_id
    @cmd_result = run_cmd(
      'mqtt-sn-pub',
      '-t' => 'topic_name',
      '-T' => 10,
      '-m' => 'message'
    )
    assert_match(/Please provide either a topic id or a topic name, not both/, @cmd_result[0])
  end

  def test_both_message_and_file
    @cmd_result = run_cmd(
      'mqtt-sn-pub',
      '-t' => 'topic_name',
      '-m' => 'message',
      '-f' => '/dev/zero'
    )
    assert_match(/Please provide either message data or a message file, not both/, @cmd_result[0])
  end

  def test_file_doesnt_exist
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-pub',
        '-t' => 'topic_name',
        '-f' => '/doesnt/exist',
        '-p' => fs.port,
        '-h' => fs.address
      ) do |cmd|
        wait_for_output_then_kill(cmd)
      end
    end
    assert_match(/Failed to open message file/, @cmd_result[0])
  end

  def test_both_qos_n1_topic_name
    @cmd_result = run_cmd(
      'mqtt-sn-pub',
      '-q' => -1,
      '-t' => 'topic_name',
      '-m' => 'message'
    )
    assert_match(/Either a pre-defined topic id or a short topic name must be given for QoS -1/, @cmd_result[0])
  end

  def test_topic_name_too_long
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-pub',
        ['-t', 'x' * 255,
        '-m', 'message',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        wait_for_output_then_kill(cmd)
      end
    end
    assert_match(/ERROR Topic name is too long/, @cmd_result[0])
  end

  def test_register_invalid_topic_name
    fake_server do |fs|
      def fs.handle_register(packet)
        MQTT::SN::Packet::Regack.new(
          :id => packet.id,
          :return_code => 2
        )
      end

      @cmd_result = run_cmd(
        'mqtt-sn-pub',
        ['-t', '/!invalid%topic"name',
        '-m', 'message',
        '-p', fs.port,
        '-h', fs.address]
      ) do |cmd|
        wait_for_output_then_kill(cmd)
      end
    end

    assert_match(/ERROR REGISTER failed: Rejected: invalid topic ID/, @cmd_result[0])
  end

  def test_connect_fail
    skip("Unable to get this test to run reliably")
#     @cmd_result = run_cmd(
#       'mqtt-sn-pub',
#       ['-t', 'topic',
#       '-m', 'message',
#       '-h', '0.0.0.1',
#       '-p', '29567']
#     ) do |cmd|
#       wait_for_output_then_kill(cmd)
#     end
# 
#     assert_match(/ERROR Could not connect to remote host/, @cmd_result[0])
  end

  def test_hostname_lookup_fail
    @cmd_result = run_cmd(
      'mqtt-sn-pub',
      ['-t', 'topic',
      '-m', 'message',
      '-p', '29567',
      '-h', '!(invalid)']
    ) do |cmd|
      wait_for_output_then_kill(cmd)
    end

    assert_match(/nodename nor servname provided, or not known|Name or service not known/, @cmd_result[0])
  end

  def test_disconnect_duration_warning
    fake_server do |fs|
      def fs.handle_disconnect(packet)
        MQTT::SN::Packet::Disconnect.new(
          :duration => 10
        )
      end

      @cmd_result = run_cmd(
        'mqtt-sn-pub',
        '-T' => 10,
        '-m' => 'message',
        '-p' => fs.port,
        '-h' => fs.address
      )
    end

    assert_match(/DISCONNECT warning. Gateway returned duration in disconnect packet/, @cmd_result[0])
  end

end
