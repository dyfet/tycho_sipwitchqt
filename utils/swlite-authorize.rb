#!/usr/bin/env ruby
# Copyright (C) 2017 Tycho Softworks
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

['optparse', 'io/console', 'sqlite3', 'digest'].each {|mod| require mod}

if STDIN.respond_to?(:noecho)
  def get_password(prompt="Password: ")
    print prompt
    STDIN.noecho(&:gets).chomp
  end
else
  def get_password(prompt="Password: ")
    `read -s -p "#{prompt}" password; echo $password`.chomp
  end
end

access = 'LOCAL'
digest = 'MD5'
number = -1
minext = 100
maxext = 699
user = nil
password = nil
mode = 'USER'
create = false
domain = nil
dbname = '/var/lib/sipwitchqt/local.db'
dbname = '../testdata/local.db' if File.writable?('../testdata/local.db')
dbname = '../userdata/local.db' if File.writable?('../userdata/local.db')
abort("*** swlite-authorize: no database") unless File.writable?(dbname)

OptionParser.new do |opts|
  opts.banner = 'Usage: swlite-authorize [id]'

  opts.on('-c', '--create', 'deauthorize and remove user') do
    create = true
  end

  opts.on('-d', '--drop', 'deauthorize and remove user') do
    digest = 'DROP'
  end

  opts.on('-l', '--local', 'local device group') do
    mode = 'DEVICE'
    access = 'LOCAL'
  end

  opts.on('-p', '--password [SECRET]', 'force password set') do |secret|
    password = secret
  end

  opts.on('-r', '--remote', 'remote user access') do
    access = 'REMOTE'
  end

  opts.on('-s', '--suspend', 'suspend user') do
    digest = 'NONE'
  end

  opts.on('-t', '--team', 'team authorization') do
    mode = 'TEAM'
    access = 'TEAM'
  end

  opts.on('-2', '--sha256', 'use sha-256') do
    digest = 'SHA-256'
  end

  opts.on('-5', '--sha512', 'use sha-512') do
    digest = 'SHA-512'
  end

  opts.on_tail('-h', '--help', 'Show this message') do
    puts opts
    exit
  end
end.parse!
abort(opts.banner) if(ARGV.size > 1)
user = ARGV[0]

begin 
  db = SQLite3::Database.open dbname
  db.execute("SELECT realm, dialing FROM Config") do |row|
    abort("*** swlite-authorize: multiple config entries") unless domain == nil
    domain = row[0]
    dialing = row[1]
    case dialing
    when 'STD3'
      minext = 100
      maxext = 699
    else
      abort("*** swlite-authorize: #{dialing}: invalid dialing plan used")
    end
  end
rescue SQLite3::SQLException
  abort("*** swlite-authorize: no switch table")
end

print "Realm #{domain}\n"
exit if user == nil

begin
  type = 'NONE'
  db.execute("SELECT name, type, access FROM Authorize WHERE name='#{user}'") do |row|
    type = row[1]
    access = row[2]
  end
  if type == 'NONE'
    abort("*** swlite-authorize: #{user}: no such authorization") if create != true or digest == 'DROP'
    db.execute("INSERT INTO Authorize (name, type, realm, access) VALUES('#{user}','#{mode}','#{domain}','#{access}')")
    type = mode
    print "Created #{user}\n"
  end
  case digest
  when 'NONE'
    print "Suspending authorization #{user}\n"
    db.execute("UPDATE Authorize set digest='NONE', secret='', realm='#{domain}' WHERE name='#{user}'")
  when 'DROP'
    case type
    when 'USER', 'DEVICE', 'TEAM'
      print "Dropping authorization #{user}\n"
      db.execute("DELETE FROM Authorize WHERE name='#{user}'")
    else
      abort("*** swlite-authorize: #{user}: cannot drop authorization as #{type}")
    end
  else
    case access
    when 'LOCAL', 'REMOTE', 'TEAM'
      print "Changing password for #{user}\n"
      if password == nil
        pass1 = get_password
        print "\n"
        exit unless pass1.size > 0
        pass2 = get_password "Retype Password: "
        print "\n"
      else
        pass1 = pass2 = password
      end
      exit unless pass2.size > 0
      abort("*** swlite-authorize: passwords do not match") unless pass1 == pass2
      case digest
      when 'SHA-256'
        secret = Digest::SHA256.hexdigest "#{user}:#{domain}:#{pass1}"
      when 'SHA-512'
        secret = Digest::SHA512.hexdigest "#{user}:#{domain}:#{pass1}"
      else
        secret = Digest::MD5.hexdigest "#{user}:#{domain}:#{pass1}"
      end
      db.execute("UPDATE Authorize set digest='#{digest}', secret='#{secret}', realm='#{domain}' WHERE name='#{user}'")
    else
      abort("*** swlite-authorize: #{user}: not able to authorize as #{access}")
    end
  end
rescue SQLite3::BusyException
  abort("*** swlite-authorize: database busy; sipwitchqt active")
rescue SQLite3::SQLException
  abort("*** swlite-authorize: database error")
rescue Interrupt
  abort("^C")
  exit
end

