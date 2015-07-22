$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnSubTest < Minitest::Test

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-sub', '-?')
    assert_match /^Usage: mqtt-sn-sub/, @cmd_result[0]
  end

  def test_subscribe_one
    fake_server do |fs|
      @cmd_result = run_cmd(
        'mqtt-sn-sub',
        ['-1', '-v',
        '-t', 'test',
        '-p', fs.port]
      )
    end

    assert_equal ["test: Hello World\n"], @cmd_result
  end

end
