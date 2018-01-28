#!/usr/bin/env ruby
# Copyright (C) 2018 Tycho Softworks
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

['sqlite3'].each {|mod| require mod}

dbname = '/var/lib/sipwitchqt/local.db'
dbname = '../testdata/local.db' if File.writable?('../testdata/local.db')
dbname = '../userdata/local.db' if File.writable?('../userdata/local.db')
abort("*** swlite-authorize: no database") unless File.writable?(dbname)

begin
  db = SQLite3::Database.open dbname
  db.execute("SELECT name, type, access FROM Authorize ORDER BY name") do |row|
    puts "%-32s %-8s %-8s" % row
  end
rescue SQLite3::BusyException
  abort("*** swlite-authorize: database busy; sipwitchqt active")
rescue SQLite3::SQLException
  abort("*** swlite-authorize: database error")
end
