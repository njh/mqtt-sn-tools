$:.unshift(File.join(File.dirname(__FILE__),'..','lib'))

require 'rubygems'
require 'bundler'
Bundler.require(:default)
require 'minitest/autorun'
require 'fake_server'

CMD_DIR = File.realpath('../..', __FILE__)

def run_cmd(name, args=[], data=nil)
  args = [args] unless args.respond_to?(:flatten)
  cmd = [CMD_DIR + '/' + name] + args.flatten.map {|i| i.to_s}
  IO.popen([*cmd, :err => [:child, :out]], 'r+b') do |io|
    if block_given?
      yield(io)
    end
    unless data.nil?
      io.write data
    end
    io.close_write
    io.readlines.map {|line| line.strip}
  end
end

def wait_for_output_then_kill(io, signal='INT', timeout=0.1)
  IO.select([io], nil, nil, timeout)
  sleep(0.1)
  Process.kill(signal, io.pid)
end

def random_port
  10000 + ((rand(10000) + Time.now.to_i) % 10000)
end

def fake_server(*args)
  fs = MQTT::SN::FakeServer.new(*args)
  fs.logger.level = Logger::WARN
  fs.start
  fs.wait_for_port_number
  yield(fs)
  fs.stop
  return fs
end

def have_ipv6?
  Socket.ip_address_list.any? { |addr| addr.ipv6? }
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
