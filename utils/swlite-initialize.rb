#!/usr/bin/env ruby
# Copyright (C) 2017-2018 Tycho Softworks
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

['optparse', 'io/console', 'sqlite3', 'digest'].each {|mod| require mod}

RESERVED_NAMES = ['operators', 'system', 'anonymous', 'nobody']
database = 'sqlite'
digest_type = 'MD5'
first = '100'
last = '699'

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

domain = nil
dbname = '/var/lib/sipwitchqt/local.db'
dbname = '../testdata/local.db' if File.writable?('../testdata/local.db')
dbname = '../userdata/local.db' if File.writable?('../userdata/local.db')
abort("*** swlite-initialize: no database") unless File.writable?(dbname)

OptionParser.new do |opts|
  opts.banner = 'Usage: swlite-initialize'

  opts.on('-2', '--sha256', 'use sha-256') do
    digest_type = 'SHA-256'
  end

  opts.on('-5', '--sha512', 'use sha-512') do
    digest_type = 'SHA-512'
  end
end.parse!
abort(opts.banner) if(ARGV.size > 0)

begin 
  db = SQLite3::Database.open dbname
  db.execute("SELECT realm, dialplan FROM Config") do |row|
    abort("*** swlite-initialize: multiple config entries") unless domain == nil
    domain = row[0]
    dialing = row[1]
    case dialing
    when 'STD3'
      first = '100'
      last = '699'
    else
      abort("*** swlite-initialize: #{dialing}: invalid dialing plan used")
    end
  end
rescue SQLite3::SQLException
  abort("*** swlite-initialize: no switch table")
end

print "Realm #{domain}\n" if domain != nil
realm = domain if domain != nil

begin
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
rescue Interrupt
  abort("^C")
  exit
end

begin
  db = SQLite3::Database.open dbname
  db.execute("INSERT INTO Authorize(authname,authdigest,realm,secret,fullname,authtype,authaccess,email) VALUES('#{user}','#{digest_type}','#{realm}','#{extpass}','#{display}','USER','REMOTE','#{email}')")
  db.execute("INSERT INTO Extensions(extnbr,authname) VALUES(#{extnbr},'#{user}')")
  db.execute("INSERT INTO Endpoints(extnbr) VALUES(#{extnbr})")
  db.execute("INSERT INTO Admin(authname,extnbr) VALUES('system',#{extnbr})")
rescue SQLite3::SQLException
  abort("*** swlite-initialize: cannot initialize #{extnbr}")
end

