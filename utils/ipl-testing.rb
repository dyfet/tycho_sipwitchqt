#!/usr/bin/env ruby
# Copyright (C) 2017 Tycho Softworks
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

# THIS IS USED TO LOAD A TEST DATA SET ONLY!!!

['optparse', 'io/console', 'digest', 'fileutils'].each {|mod| require mod}

Dir.chdir(File.dirname($0))

database = 'sqlite'

config = '/nonexistent'
config = '../testdata/service.conf' if File.exists?('../testdata/service.conf')
config = '../userdata/service.conf' if File.exists?('../userdata/service.conf')
localdb = '../testdata/local.db'
localdb = '../userdata/local.db' if File.exists?('../userdata/service.conf')
abort("*** ipl-testing: no config") unless File.exists?(config)

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
            abort("*** ipl-testing: #{value}: unknown driver")
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
testdata = "../Database/testdata.sql"
abort("*** ipl-testing: no schema") unless File.exists?(schema)
abort("*** ipl-testing: no test config") unless File.exists?(testdata)

print "Loading #{database}...\n"
