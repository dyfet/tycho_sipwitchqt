/*
 * Copyright 2017 Tycho Softworks.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.hpp"
#include "sqldriver.hpp"

#include <QStringList>

static QStringList sqliteTables = {
    "CREATE TABLE Config ("
        "id INTEGER PRIMARY KEY,"               // rowid in sqlite
        "realm VARCHAR(128) NOT NULL,"          // site realm
        "series INTEGER DEFAULT 9,"             // site db series
        "dialing VARCHAR(8) DEFAULT 'STD3');",  // site dialing plan

    "CREATE TABLE Switches ("
        "version VARCHAR(8) NOT NULL,"          // db series # supported
        "uuid CHAR(36) NOT NULL,"               // switch uuid
        "last DATETIME,"
        "PRIMARY KEY (uuid));",

    "CREATE TABLE Authorize ("
        "name VARCHAR(32),"                     // authorizing user or group id
        "type VARCHAR(8) DEFAULT 'USER',"       // group type
        "digest VARCHAR(8) DEFAULT 'NONE',"     // digest format of secret
        "realm VARCHAR(128),"                   // realm used for secret
        "secret VARCHAR(128),"                  // secret to use
        "access VARCHAR(8) DEFAULT 'LOCAL',"    // type of access allowed (local, remote, all)
        "fullname VARCHAR(64),"                 // display name
        "fwd_away INTEGER DEFAULT -1,"          // forward offline/away
        "fwd_busy INTEGER DEFAULT -1,"          // forward busy
        "fwd_answer INTEGER DEFAULT -1,"        // forward no answer
        "PRIMARY KEY (name));",

    "CREATE TABLE Extensions ("
        "number INTEGER NOT NULL,"              // ext number
        "name VARCHAR(32) DEFAULT '@nobody',"   // group affinity
        "priority INTEGER DEFAULT 0,"           // ring/dial priority
        "restrict INTEGER DEFAULT 0,"           // outgoing restrictions
        "describe VARCHAR(64),"                 // location info
        "display VARCHAR(64),"                  // can override group display
        "PRIMARY KEY (number),"
        "FOREIGN KEY (name) REFERENCES Authorize(name) "
            "ON DELETE CASCADE);",

    "CREATE TABLE Endpoints ("
        "endpoint INTEGER PRIMARY KEY AUTOINCREMENT,"
        "number INTEGER NOT NULL,"              // extension of endpoint
        "label VARCHAR(32) DEFAULT 'NONE',"      // label id
        "agent VARCHAR(64),"                    // agent id
        "created DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "last DATETIME DEFAULT 0,"
        "FOREIGN KEY (number) REFERENCES Extensions(number) "
            "ON DELETE CASCADE);",
    "CREATE UNIQUE INDEX Registry ON Endpoints(number, label);",

    // the "system" speed dialing is system group 10-99
    // per extension group personal speed dials 01-09 (#1-#9)

    "CREATE TABLE Speeds ("
        "name VARCHAR(32),"                     // speed dial for...
        "target VARCHAR(128),"                  // local or external uri
        "number INTEGER,"                       // speed dial #
        "CONSTRAINT Dialing PRIMARY KEY (name, number),"
        "FOREIGN KEY (name) REFERENCES Authorize(name) "
            "ON DELETE CASCADE);",

    "CREATE TABLE Calling ("
        "name VARCHAR(32),"                     // group hunt is part of
        "number INTEGER,"                       // extension # to ring
        "priority INTEGER DEFAULT 0,"           // hunt group priority order
        "FOREIGN KEY (name) REFERENCES Authorize(name) "
            "ON DELETE CASCADE,"
        "FOREIGN KEY (number) REFERENCES Extensions(number) "
            "ON DELETE CASCADE);",

    "CREATE TABLE Teams ("
        "name VARCHAR(32),"                     // group tied to
        "member VARCHAR(32),"                     // group member
        "priority INTEGER DEFAULT -1,"          // coverage priority
        "CONSTRAINT Grouping PRIMARY KEY (name, member),"
        "FOREIGN KEY (name) REFERENCES Authorize(name) "
            "ON DELETE CASCADE,"
        "FOREIGN KEY (member) REFERENCES Authorize(name) "
            "ON DELETE CASCADE);",

    "CREATE TABLE Providers ("
        "contact VARCHAR(128) NOT NULL,"        // provider host uri
        "protocol VARCHAR(3) DEFAULT 'UDP',"    // providers usually udp
        "userid VARCHAR(32) NOT NULL,"          // auth code
        "passwd VARCHAR(128) NOT NULL,"         // password to hash from
        "display VARCHAR(64) NOT NULL,"         // provider short name
        "PRIMARY KEY (contact));",

    "CREATE TABLE Messages ("
        "mid INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name VARCHAR(32),"                     // authorized delivery thru
        "msgfrom VARCHAR(64),"                  // origin uri direct reply
        "subject VARCHAR(80),"                  // subject header used
        "display VARCHAR(64),"                  // origin display name
        "groupid VARCHAR(32) DEFAULT NULL,"     // if group/team was target
        "posted TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "expires INTEGER DEFAULT 0,"            // carried expires header
        "msgtype VARCHAR(8),"
        "msgtext TEXT,"
        "FOREIGN KEY(name) REFERENCES Authorize(name) "
            "ON DELETE CASCADE);",

    "CREATE TABLE Outboxes ("
        "ordering INTEGER PRIMARY KEY AUTOINCREMENT,"
        "mid INTEGER,"
        "endpoint INTEGER,"
        "delivered TIMESTAMP DEFAULT 0,"
        "msgto VARCHAR(64),"                    // synthesized to header
        "FOREIGN KEY (mid) REFERENCES Messages(mid) "
            "ON DELETE CASCADE,"
        "FOREIGN KEY (endpoint) REFERENCES Endpoints(endpoint) "
            "ON DELETE CASCADE);",
    "CREATE UNIQUE INDEX Pending ON Outboxes(ordering) WHERE delivered = 0;",
};

static QStringList sqlitePragmas = {
    "PRAGMA locking_mode = EXCLUSIVE;",
    "PRAGMA synchronous = OFF;",
    "PRAGMA temp_store = MEMORY;",
};

#ifdef PRELOAD_DATABASE
static QStringList sqlitePreload = {
    // "test1" and "test2", for extensions 101 and 102, password is "testing"
    "INSERT INTO Authorize(name, digest, realm, secret, fullname, type, access) "
        "VALUES('test1','MD5','testing','74d0a5bd38ed78708aacb9f056e40120','Test User #1','USER','LOCAL');",
    "INSERT INTO Authorize(name, digest, realm, secret, fullname, type, access) "
        "VALUES('test2','MD5','testing','6d292c665b1ed72b8bfdbb5d45173d98','Test User #2','USER','LOCAL');",
    "INSERT INTO Extensions(number, name) VALUES(101, 'test1');",
    "INSERT INTO Extensions(number, name) VALUES(102, 'test2');",
    "INSERT INTO Endpoints(number) VALUES(101);",
    "INSERT INTO Endpoints(number) VALUES(102);",
};
#else
static QStringList sqlitePreload = {
};
#endif

namespace Util {
    const QStringList pragmaQuery(const QString& name)
    {
        if(name == "QSQLITE")
            return sqlitePragmas;
        else
            return QStringList();
    }

    const QStringList createQuery(const QString& name)
    {
        if(name == "QSQLITE")
            return sqliteTables;
        else
            return QStringList();
    }

    bool dbIsFile(const QString& name)
    {
        if(name == "QSQLITE")
            return true;
        else
            return false;
    }

    const QStringList preloadConfig(const QString& name)
    {
        if(name == "QSQLITE")
            return sqlitePreload;
        else
            return QStringList();
    }
}
