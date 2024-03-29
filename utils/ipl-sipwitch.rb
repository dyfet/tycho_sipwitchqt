#!/usr/bin/env ruby
# Copyright (C) 2017-2018 Tycho Softworks
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

['pp', 'optparse', 'io/console', 'digest', 'fileutils', 'mysql2'].each {|mod| require mod}

RESERVED_NAMES = ['operators', 'system', 'anonymous', 'nobody'].freeze
database = 'mysql'
digest_type = 'MD5'
first = '100'
last = '699'
config = '/nonexist'

Dir.chdir(File.dirname($PROGRAM_NAME))

if STDIN.respond_to?(:noecho)
  def get_pass(prompt='Password: ')
    print prompt
    input = STDIN.noecho(&:gets).chomp
    exit if input.empty?
    print "\n"
    input
  end
else
  def get_pass(prompt='Password: ')
    input = `read -s -p "#{prompt}" password; echo $password`.chomp
    exit if input.empty?
    print "\n"
    input
  end
end

def get_input(*args)
  print(*args)
  input = gets.chomp
  exit if input.empty?
  input
end

def get_secret(type, user, domain, pass)
  case type
  when 'SHA-256'
    Digest::SHA256.hexdigest "#{user}:#{domain}:#{pass}"
  when 'SHA-512'
    Digest::SHA512.hexdigest "#{user}:#{domain}:#{pass}"
  else
    Digest::MD5.hexdigest "#{user}:#{domain}:#{pass}"
  end
end

OptionParser.new do |opts|
  opts.banner = 'Usage: ipl-sipwitch'

  opts.on('-2', '--sha256', 'use sha-256') do
    digest_type = 'SHA-256'
  end

  opts.on('-5', '--sha512', 'use sha-512') do
    digest_type = 'SHA-512'
  end

  opts.on('-c', '--config [PATH]', 'set config file') do |path|
    config = path
  end

  opts.on('-e', '--echo', 'echo commands to stdout') do
    _echo_flag = true
  end
end.parse!
abort(opts.banner) unless ARGV.empty?

unless File.exist?(config)
  config = '/etc/sipwitchqt.conf'
  config = '../testdata/service.conf' if File.exist?('../testdata/service.conf')
  config = '../userdata/service.conf' if File.exist?('../userdata/service.conf')
end

abort('*** ipl-sipwitch: no config') unless File.exist?(config)
print "config file #{config} used\n"

section = nil
realm = nil
dbcfg = {host: '127.0.0.1', name: 'sipwitch', port: '3306', username: 'sipwitch', password: nil}

File.open(config, 'r') do |infile; line, key, value|
  while (line = infile.gets)
    line.gsub!(/(^|\s)[#].*$/, '')
    case line.strip!
    when /^\[.*\]$/
      section = line[1..-2].downcase
    when /[=]/
      key, value = line.split(/\=/).each {|s| s.strip!}
      key.downcase!
      case section
      when nil
        case key
        when 'realm'
          realm = value
        when 'database'
          case value.downcase
          when /mysql/
            database = 'mysql'
          when /sqlite/
            database = 'sqlite'
          else
            abort("*** ipl-sipwitch: #{value}: unknown driver")
          end
        end
      when 'database'
        dbcfg[key.to_sym] = value
      end
    end
  end
end

# find schema
schema = "../Database/#{database}.sql"
schema = "../share/schemas/sipwitch-#{database}.sql" if File.exist?("/usr/share/schemas/sipwitch-#{database}.sql")
abort('*** ipl-sipwitch: no schema') unless File.exist?(schema)

# gather facts for initial config....

begin
  print "Creating realm \"#{realm}\" for #{database}\n" unless realm.nil? || realm.empty?
  realm   = get_input 'Server realm: ' if realm.empty?
  user    = get_input 'Authorizing user: '
  abort("*** ipl-sipwitch: #{user}: reserved name") if RESERVED_NAMES.include?(user)
  extnbr  = get_input 'User extension:   '
  abort("*** ipl-sipwitch: ext must be #{first}-#{last}") unless extnbr >= first && extnbr <= last
  display = get_input 'Display name:     '
  email = get_input   'Email address:    '
rescue Interrupt
  abort('')
end

begin
  print "\nEnter a password for #{extnbr} to authorize with\n"
  pass1    = get_pass 'Password: '
  pass2    = get_pass 'Verify:   '
  abort('*** ipl-sipwitch: passwords do not match') unless pass1 == pass2
  extpass = get_secret(digest_type, user, realm, pass1)
rescue Interrupt
  abort('^C')
end

IPL_COMMANDS = [
  "INSERT INTO Config(id, realm) VALUES(1,'#{realm}');",
  "INSERT INTO Authorize(authname,authtype,authaccess) VALUES('system', 'SYSTEM', 'LOCAL');",
  "INSERT INTO Authorize(authname,authtype,authaccess) VALUES('nobody', 'SYSTEM', 'LOCAL');",
  "INSERT INTO Authorize(authname,authtype,authaccess) VALUES('anonymous', 'SYSTEM', 'DISABLED');",
  "INSERT INTO Authorize(authname,authtype,authaccess) VALUES('operators', 'SYSTEM', 'PILOT');",
  "INSERT INTO Extensions(extnbr,authname,display) VALUES(0, 'operators', 'Operators');",
  "INSERT INTO Authorize(authname,authdigest,realm,secret,fullname,authtype,authaccess,email) VALUES('#{user}','#{digest_type}','#{realm}','#{extpass}','#{display}','USER','REMOTE','#{email}');",
  "INSERT INTO Extensions(extnbr,authname) VALUES(#{extnbr},'#{user}');",
  "INSERT INTO Endpoints(extnbr) VALUES(#{extnbr});",
  "INSERT INTO Admin(authname,extnbr) VALUES('system',#{extnbr});",
].freeze

line = ''
dbcfg[:database] = dbcfg[:name]

begin
  pp dbcfg
  db = Mysql2::Client.new(dbcfg)
  File.open(schema, 'r') do |infile; cmd|
    while (cmd = infile.gets)
      cmd.gsub!(/(^|\s)[-][-].*$/, '')
      case cmd.strip!
      when /^$/
        line.strip!
        db.query(line) if line.length > 1
        line = ''
      else
        line = "#{line} #{cmd}"
      end
    end
  end
  IPL_COMMANDS.each do |cmd|
    db.query(cmd)
  end
rescue Mysql2::Error => e
  abort("*** ipl-sipwitch: mysql error=#{e.errno},#{e.error}")
end
