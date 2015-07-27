$:.unshift(File.join(File.dirname(__FILE__),'..','lib'))

require 'rubygems'
require 'bundler'
Bundler.require(:default)
require 'minitest/autorun'
require 'fake_server'

CMD_DIR = File.realpath('../..', __FILE__)

def run_cmd(name, args=[])
  args = [args] unless args.respond_to?(:flatten)
  cmd = [CMD_DIR + '/' + name] + args.flatten.map {|i| i.to_s}
  IO.popen([*cmd, :err => [:child, :out]], 'rb') do |io|
    if block_given?
      yield(io)
    end
    io.readlines.map {|line| line.strip}
  end
end

def fake_server
  fs = MQTT::SN::FakeServer.new
  fs.logger.level = Logger::WARN
  fs.start
  fs.wait_for_port_number
  yield(fs)
  fs.stop
end

module Minitest::Assertions
  def assert_includes_match(regexp, array, msg=nil)
    msg = message(msg) { "Expected #{mu_pp(array)} to match #{mu_pp(regexp)}" }
    array.each do |item|
      if item =~ regexp
        assert true, msg
        return
      end
    end
    assert false, msg
  end
end
