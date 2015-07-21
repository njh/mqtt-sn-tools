$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnPubTest < MiniTest::Unit::TestCase

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-pub', '-?')
    assert_match /^Usage: mqtt-sn-pub/, @cmd_result[0]
  end

  def test_publish_qos_n1
    fake_server do |fs|
      @packet = fs.wait_for_publish do
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
  end

  def test_publish_qos_0
    fake_server do |fs|
      @packet = fs.wait_for_publish do
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
  end

  def test_publish_qos_0_short
    fake_server do |fs|
      @packet = fs.wait_for_publish do
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
  end

  def test_publish_qos_0_predefined
    fake_server do |fs|
      @packet = fs.wait_for_publish do
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
  end

end
