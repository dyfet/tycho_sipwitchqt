#!/usr/bin/env ruby
# Copyright (C) 2017-2018 Tycho Softworks
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

['optparse', 'openssl'].each {|mod| require mod}

banner = 'Usage: swcert-read [options]'
output = false

path = '/var/lib/sipwitchqt/public.pem'
path = '../testdata/public.pem' if File.directory? '../testdata'

OptionParser.new do |opts|
  opts.banner = banner

  opts.on('-u', '--output', 'output to stdout') do
    output = true
  end

  opts.on('-u', '--userdata', 'read from userdata directory') do
    path = '../userdata/public.pem'
  end

  opts.on_tail('-h', '--help', 'Show this message') do
    puts opts
    exit
  end
end.parse!

abort(banner) unless ARGV.empty?

raw = File.read path
cert = OpenSSL::X509::Certificate.new raw

subject = cert.subject.to_a
server = subject.select {|name, _, _| name == 'CN'}.first[1]
country = subject.select {|country, _, _| country == 'C'}.first[1]
organization = subject.select {|organization, _, _| organization == 'O'}.first[1]

if output
  puts raw
else
  print "#{server} #{organization} #{country}\n"
end
