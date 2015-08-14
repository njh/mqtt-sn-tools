$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnPubTest < Minitest::Test

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-pub', '-?')
    assert_match /^Usage: mqtt-sn-pub/, @cmd_result[0]
  end

  def test_no_arguments
    @cmd_result = run_cmd('mqtt-sn-pub')
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
          '-p' => fs.port,
          '-h' => fs.address
        )
      end
    end

    assert_empty @cmd_result
    assert_equal 'test_custom_client_id', @packet.client_id
    assert_equal 30, @packet.keep_alive
    assert_equal true, @packet.clean_session
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

    assert_match /ERROR Client id is too long/, @cmd_result[0]
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
          '-p', fs.port,
          '-h', fs.address]
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
          '-p' => fs.port,
          '-h' => fs.address
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
          '-p' => fs.port,
          '-h' => fs.address
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
          '-p' => fs.port,
          '-h' => fs.address
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
          '-p', fs.port,
          '-h', fs.address]
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
          '-p', fs.port,
          '-h', fs.address]
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

  def test_invalid_qos
    @cmd_result = run_cmd(
    	'mqtt-sn-pub',
    	'-q' => '1',
    	'-t' => 'topic',
    	'-m' => 'message'
    )
    assert_match /Only QoS level 0 or -1 is supported/, @cmd_result[0]
  end

  def test_both_topic_name_and_id
    @cmd_result = run_cmd(
    	'mqtt-sn-pub',
    	'-t' => 'topic_name',
    	'-T' => 10,
    	'-m' => 'message'
    )
    assert_match /Please provide either a topic id or a topic name, not both/, @cmd_result[0]
  end

  def test_both_qos_n1_topic_name
    @cmd_result = run_cmd(
    	'mqtt-sn-pub',
    	'-q' => -1,
    	'-t' => 'topic_name',
    	'-m' => 'message'
    )
    assert_match /Either a pre-defined topic id or a short topic name must be given for QoS -1/, @cmd_result[0]
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
    assert_match /ERROR Topic name is too long/, @cmd_result[0]
  end

end
