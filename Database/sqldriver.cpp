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
        "PRIMARY KEY (name));",

    "CREATE TABLE Extensions ("
        "number INTEGER NOT NULL,"              // ext number
        "name VARCHAR(32) DEFAULT '@nobody',"   // group affinity
        "priority INTEGER DEFAULT 0,"           // ring/dial priority
        "restrict INTEGER DEFAULT 0,"           // outgoing restrictions
        "describe VARCHAR(64),"                 // location info
        "display VARCHAR(64),"                  // can override group display
        "offline INTEGER,"                      // forwarding offline
        "away INTEGER,"                         // forwarding away
        "busy INTEGER,"                         // forwarding busy
        "noanswer INTEGER,"                     // forwarding no answer
        "PRIMARY KEY (number),"
        "FOREIGN KEY (name) REFERENCES Authorize(name) "
            "ON DELETE CASCADE);",

    "CREATE TABLE Endpoints ("
        "endpoint INTEGER PRIMARY KEY AUTOINCREMENT,"
        "number INTEGER NOT NULL,"              // extension of endpoint
        "label VARCHAR(32) DEFAULT 'PHONE',"    // label id
        "agent VARCHAR(64),"                    // agent id
        "created DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "last DATETIME,"
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
};

static QStringList sqlitePragmas = {
    "PRAGMA locking_mode = EXCLUSIVE;",
    "PRAGMA synchronous = OFF;",
    "PRAGMA temp_store = MEMORY;",
};

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
}
