#!/usr/bin/env ruby
# Copyright (C) 2017-2018 Tycho Softworks
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

['optparse', 'io/console', 'digest', 'fileutils'].each {|mod| require mod}

RESERVED_NAMES = ['operators', 'system', 'anonymous', 'nobody']
database = 'sqlite'
digest_type = 'MD5'
first = '100'
last = '699'
config = '/nonexist'

Dir.chdir(File.dirname($0))

if STDIN.respond_to?(:noecho)
  def get_pass(prompt="Password: ")
    print prompt
    input = STDIN.noecho(&:gets).chomp
    exit unless input.size > 0
    print "\n"
    input
  end
else
  def get_pass(prompt="Password: ")
    input = `read -s -p "#{prompt}" password; echo $password`.chomp
    exit unless input.size > 0
    print "\n"
    input
  end
end

def get_input(*args)
  print(*args)
  input = gets.chomp
  exit unless input.size > 0
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
end.parse!
abort(opts.banner) if(ARGV.size > 0)

if !File.exists?(config) 
  config = '/etc/sipwitchqt.conf'
  config = '../testdata/service.conf' if File.exists?('../testdata/service.conf')
  config = '../userdata/service.conf' if File.exists?('../userdata/service.conf')
  localdb = '/var/lib/sipwitchqt/local.db'
end

abort("*** ipl-sipwitch: no config") unless File.exists?(config)
print "config file #{config} used\n"

section = nil
realm = nil
db = {:database => 'localhost', :port => '3306', :username => 'sipwitch', :password => nil}

File.open(config, 'r') do |infile; line, key, value|
  while(line = infile.gets)
    line.gsub!(/(^|b)[#].*$/, '')
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
        db[key.to_sym] = value
      end
    end
  end
end

# find schema
schema = "../Database/#{database}.sql"
schema = "../share/schemas/sipwitch-#{database}.sql" if File.exists?("/usr/share/schemas/sipwitch-#{database}.sql")
abort("*** ipl-sipwitch: no schema") unless File.exists?(schema)

# gather facts for initial config....

begin
  print "Creating realm \"#{realm}\" for #{database}\n" if realm.size > 0
  realm   = get_input "Server realm: " if realm === nil
  user    = get_input "Authorizing user: " 
  abort("*** ipl-sipwitch: #{user}: reserved name") if RESERVED_NAMES.include?(user)
  extnbr  = get_input "User extension:   "
  abort("*** ipl-sipwitch: ext must be #{first}-#{last}") unless extnbr >= first and extnbr <= last
  display = get_input "Display name:     "
  email = get_input   "Email address:    "
rescue Interrupt
  abort("")
  exit
end

begin
  print "\nEnter a password for #{extnbr} to authorize with\n"
  pass1    = get_pass "Password: "
  pass2    = get_pass "Verify:   "
  abort("*** ipl-sipwitch: passwords do not match") unless pass1 == pass2
  extpass = get_secret(digest_type,user,realm,pass1)

  print "\nEnter a password for new users to register with\n"
  pass1    = get_pass "Password: "
  pass2    = get_pass "Verify:   "
  abort("*** ipl-sipwitch: passwords do not match") unless pass1 == pass2
  regpass = get_secret(digest_type,'anonymous',realm,pass1)
  p extpass, regpass
rescue Interrupt
  abort("^C")
  exit
end

# generate database sql from a ?templated? sql file...to be here...
ipl = File.open('/tmp/ipl.sql', 'w')
ipl << "\n-- Facts gathered database config\n"
ipl << "INSERT INTO Config(realm) VALUES('#{realm}');\n"
ipl << "\n-- Default system groups and users\n"
ipl << "INSERT INTO Authorize(authname,authtype,authaccess) VALUES('system', 'SYSTEM', 'LOCAL');\n"
ipl << "INSERT INTO Authorize(authname,authtype,authaccess) VALUES('nobody', 'SYSTEM', 'LOCAL');\n"
ipl << "INSERT INTO Authorize(authname,authtype,authaccess,realm,secret,authdigest) VALUES('anonymous', 'SYSTEM', 'LOCAL','#{realm}','#{regpass}',#{digest_type}');\n"
ipl << "INSERT INTO Authorize(authname,authtype,authaccess) VALUES('operators', 'SYSTEM', 'PILOT');\n"
ipl << "INSERT INTO Extensions(extnbr,authname,display) VALUES(0, 'operators', 'Operators');\n"
ipl << "\n-- Facts gathered first/admin user\n"
ipl << "INSERT INTO Authorize(authname,authdigest,realm,secret,fullname,authtype,authaccess,email) VALUES('#{user}','#{digest_type}','#{realm}','#{extpass}','#{display}','USER','REMOTE','#{email}');\n"
ipl << "INSERT INTO Extensions(extnbr,authname) VALUES(#{extnbr},'#{user}');\n"
ipl << "INSERT INTO Endpoints(extnbr) VALUES(#{extnbr});\n"
ipl << "INSERT INTO Admin(authname,extnbr) VALUES('#{user}',#{extnbr});\n"
ipl.close

