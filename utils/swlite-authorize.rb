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

banner = 'Usage: swlite-authorize [[extension-to-create] id]'
digest = 'MD5'
number = -1
minext = 100
maxext = 699
user = nil
domain = nil
dbname = '/var/lib/sipwitchqt/local.db'
dbname = '../testdata/local.db' if File.writable?('../testdata/local.db')
abort("*** swlite-authorize: no database") unless File.writable?(dbname)

OptionParser.new do |opts|
  opts.banner = banner

  opts.on('d', '--drop', 'deauthorize and remove user') do
    digest = 'DROP'
  end

  opts.on('-s', '--suspend', 'suspend user') do
    digest = 'NONE'
  end

  opts.on('-u', '--userdata', 'db in userdata directory') do
    dbname = '../userdata/local.db'
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
abort(banner) if(ARGV.size < 0 || ARGV.size > 2)

if ARGV.size > 1
    user = ARGV[1]
    number = ARGV[0].to_i
else
    user = ARGV[0]
end

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

if number > -1
  abort("*** swlite-authorize: #{number}: extension must be #{minext}-#{maxext}") if number < minext or number > maxext
end

print "Realm #{domain}\n"
exit if user == nil

begin
  # check if creating extension and already exists...
  if number > -1
    ext = db.execute("SELECT * FROM Extensions WHERE number='#{number}'")
    abort("*** swlite-authorize: #{number}: extension already exists") if ext.size > 0
  end
  row = db.execute("SELECT userid, number FROM Authorize WHERE userid='#{user}'")
  abort("*** swlite-authorize: #{user}: multiple entries") if row.size > 1
  abort("*** swlite-authorize: #{user}: already exists") if number > -1 and row.size > 0
  if number > -1
    db.execute("INSERT INTO Extensions (number, type, alias, access, display) VALUES(#{number},'USER','#{user}','LOCAL','#{user}')")
    db.execute("INSERT INTO Authorize (userid, number, realm) VALUES('#{user}',#{number},'#{domain}')")
    row = db.execute("SELECT userid, number FROM Authorize WHERE userid='#{user}'")
  else
    if row.size < 1
      # fix bad db case where extension for alias exists, but there is no
      # matching authorize record found
      row = db.execute("SELECT alias, number FROM Extensions WHERE alias='#{user}'")
      if row.size == 1
        number = row[0][1]
        db.execute("INSERT INTO Authorize (userid, number, realm) VALUES('#{user}',#{number},'#{domain}')") 
      end
    else
      number = row[0][1] if row.size > 0
    end
  end
  case digest
  when 'NONE'
    abort("*** swlite-authorize: #{user}: no such user") unless row.size > 0
    print "Suspending user #{user}\n"
    db.execute("UPDATE Authorize set digest='NONE', secret='', realm='#{domain}' WHERE userid='#{user}'")
  when 'DROP'
    abort("*** swlite-authorize: #{user}: no such user") unless row.size > 0
    abort("*** swlite-authorize: #{number}: extension must be #{minext}-#{maxext}") if number < minext or number > maxext
    print "Dropping user #{user}\n"
    db.execute("DELETE FROM Extensions WHERE number='#{number}'")
  else
    abort("*** swlite-authorize: #{user}: no such user") unless row.size > 0
    print "Changing password for #{user}\n"
    pass1 = get_password
    print "\n"
    exit unless pass1.size > 0
    pass2 = get_password "Retype Password: "
    print "\n"
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
    db.execute("UPDATE Authorize set digest='#{digest}', secret='#{secret}', realm='#{domain}' WHERE userid='#{user}'")
  end
rescue SQLite3::SQLException
  abort("*** swlite-authorize: database error")
rescue Interrupt
  abort("^C")
  exit
end

