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
    "CREATE TABLE Switches ("
        "realm VARCHAR(128) NOT NULL,"          // realm we use
        "dialing CHAR(1) DEFAULT '3',"          // dialing plan used
        "uuid CHAR(36) NOT NULL,"               // switch uuid
        "PRIMARY KEY (uuid));",

    "CREATE TABLE Extensions ("
        "number INTEGER NOT NULL,"              // ext number
        "type VARCHAR(8) DEFAULT 'NONE',"       // ext type/suspended state
        "alias VARCHAR(32) DEFAULT NULL,"       // public visible alias
        "access VARCHAR(8) DEFAULT 'LOCAL',"    // type of access allowed (local, remote, all)
        "display VARCHAR(64),"                  // display name
        "PRIMARY KEY (number));",
    "CREATE UNIQUE INDEX Aliases ON Extensions(alias) WHERE alias IS NOT NULL;",

    "CREATE TABLE Authorize ("
        "userid VARCHAR(32),"                   // authorizing user id
        "number INTEGER,"                       // extension number tied to
        "digest VARCHAR(8) DEFAULT 'MD5',"      // digest format of secret
        "realm VARCHAR(128) NOT NULL,"          // realm used for secret
        "secret VARCHAR(128),"                  // secret
        "last DATETIME,"                        // last registration
        "PRIMARY KEY (userid),"
        "FOREIGN KEY (number) REFERENCES Extensions(number));",

    "CREATE TABLE Forward ("
        "number INTEGER,"                       // extension number tied to
        "type VARCHAR(8),"                      // forward type (busy, na, etc)
        "target VARCHAR(256),"                  // destination
        "CONSTRAINT FwdType PRIMARY KEY (number, type),"
        "FOREIGN KEY (number) REFERENCES Extensions(number));",

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
