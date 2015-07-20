$:.unshift(File.join(File.dirname(__FILE__),'..','lib'))

require 'rubygems'
require 'bundler'
Bundler.require(:default)
require 'minitest/autorun'
require 'fake_server'

CMD_DIR = File.realpath('../..', __FILE__)

def run_cmd(name, args=[])
  cmd = [CMD_DIR + '/' + name] + args.flatten.map {|i| i.to_s}
  IO.popen([*cmd, :err => [:child, :out]], 'rb') do |io|
    io.readlines
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
