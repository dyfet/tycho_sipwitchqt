#!/usr/bin/env ruby
# Copyright (C) 2017-2018 Tycho Softworks
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

['optparse', 'openssl', 'fileutils'].each {|mod| require mod}

class SelfSignedCertificate
  def initialize(server = 'localhost', keysize = 1024, org='SipWitchQt', country = 'US')
    @key = OpenSSL::PKey::RSA.new(keysize)
    public_key = @key.public_key

    subject = "/C=#{country}/O=#{org}/CN=#{server}"

    @cert = OpenSSL::X509::Certificate.new
    @cert.subject = @cert.issuer = OpenSSL::X509::Name.parse(subject)
    @cert.not_before = Time.now
    @cert.not_after = Time.now + 365 * 24 * 60 * 60
    @cert.public_key = public_key
    @cert.serial = 0x0
    @cert.version = 2

    ef = OpenSSL::X509::ExtensionFactory.new
    ef.subject_certificate = @cert
    ef.issuer_certificate = @cert
    @cert.extensions = [
      ef.create_extension('basicConstraints', 'CA:TRUE', true),
      ef.create_extension('subjectKeyIdentifier', 'hash'),
    #  ef.create_extension('keyUsage', 'cRLSign,keyCertSign', true),
    ]
    @cert.add_extension ef.create_extension('authorityKeyIdentifier', 'keyid:always,issuer:always')

    @cert.sign @key, OpenSSL::Digest::SHA1.new
  end

  def public_pem
    @cert.to_pem
  end

  def private_key
    @key
  end
end

keysize = 1024
server = 'localhost'
country = 'US'
organization = 'SipWitchQt'
banner = 'Usage: swcert-create [options]'

datadir = '/var/lib/sipwitchqt'
datadir = '../testdata' if File.directory? '../testdata'

OptionParser.new do |opts|
  opts.banner = banner

  opts.on('-4', '--4096', 'specify 4096 bit keysize') do
    keysize = 4096
  end

  opts.on('-c CODE', '--country CODE', 'specify country code') do |code|
    country = code
  end

  opts.on('-o ORG', '--org ORG', 'specify certificate hostname') do |org|
    organization = org
  end

  opts.on('-s HOST', '--server HOST', 'specify certificate hostname') do |host|
    server = host
  end

  opts.on('-u', '--userdata', 'output to userdata directory') do
    datadir = '../userdata'
  end

  opts.on_tail('-h', '--help', 'Show this message') do
    puts opts
    exit
  end
end.parse!
abort(banner) unless ARGV.empty?

cert = SelfSignedCertificate.new(server, keysize, organization, country)

File.open("#{datadir}/public.pem", 'w') do |f|
  f.puts cert.public_pem
end

File.open("#{datadir}/private.key", 'w') do |f|
  f.puts cert.private_key
end

FileUtils.chmod 0o600, "#{datadir}/private.key"
