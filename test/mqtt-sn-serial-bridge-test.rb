$:.unshift(File.dirname(__FILE__))

require 'test_helper'

class MqttSnSerialBridgeTest < Minitest::Test

  def test_usage
    @cmd_result = run_cmd('mqtt-sn-serial-bridge', '-?')
    assert_match(/^Usage: mqtt-sn-serial-bridge/, @cmd_result[0])
  end

end
