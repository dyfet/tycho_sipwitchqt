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
#include <QFile>

//void createTables(){

//    QFile::open("sqlite.sql",QIODevice::ReadOnly);
//}

// TODO: remove on ipl turnover
static QStringList sqliteTables = {
    "CREATE TABLE Config ("
        "id INTEGER PRIMARY KEY,"               // rowid in sqlite
        "realm VARCHAR(128) NOT NULL,"          // site realm
        "dbseries INTEGER DEFAULT 9,"             // site db series
        "dialplan VARCHAR(8) DEFAULT 'STD3');",  // site dialing plan

    "CREATE TABLE Switches ("
        "version VARCHAR(8) NOT NULL,"          // db series # supported
        "uuid CHAR(36) NOT NULL,"               // switch uuid
        "lastaccess DATETIME,"
        "PRIMARY KEY (uuid));",

    "CREATE TABLE Authorize ("
        "authname VARCHAR(32),"                     // authorizing user or group id
        "authtype VARCHAR(8) DEFAULT 'USER',"       // group type
        "authdigest VARCHAR(8) DEFAULT 'NONE',"     // digest format of secret
        "created DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "realm VARCHAR(128),"                   // realm used for secret
        "secret VARCHAR(128),"                  // secret to use
        "authaccess VARCHAR(8) DEFAULT 'LOCAL',"    // type of access allowed (local, remote, all)
        "email VARCHAR(128),"                   // email contact to use (avatar, etc)
        "fullname VARCHAR(64),"                 // display name
        "fwdaway INTEGER DEFAULT -1,"          // forward offline/away
        "fwdbusy INTEGER DEFAULT -1,"          // forward busy
        "fwdnoanswer INTEGER DEFAULT -1,"        // forward no answer
        "PRIMARY KEY (authname));",

    "CREATE TABLE Extensions ("
        "extnbr INTEGER NOT NULL,"              // ext number
        "authname VARCHAR(32) DEFAULT '@nobody',"   // group affinity
        "extpriority INTEGER DEFAULT 0,"           // ring/dial priority
        "callaccess INTEGER DEFAULT 0,"         // outgoing restrictions
        "extlocation VARCHAR(64),"              // location info
        "display VARCHAR(64),"                  // can override group display
        "pubkey BLOB DEFAULT NULL,"             // public key to verify signed hashes
        "PRIMARY KEY (extnbr),"
        "FOREIGN KEY (authname) REFERENCES Authorize(authname) "
            "ON DELETE CASCADE);",

    "CREATE TABLE Endpoints ("
        "endpoint INTEGER PRIMARY KEY AUTOINCREMENT,"
        "extnbr INTEGER NOT NULL,"               // extension of endpoint
        "label VARCHAR(32) DEFAULT 'NONE',"      // label id
        "agent VARCHAR(64),"                    // agent id
        "devkey BLOB DEFAULT NULL,"             // devices public key
        "created DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "lastaccess DATETIME DEFAULT 0,"
        "FOREIGN KEY (extnbr) REFERENCES Extensions(extnbr) "
            "ON DELETE CASCADE);",
    "CREATE UNIQUE INDEX Registry ON Endpoints(extnbr, label);",

    "CREATE TABLE Deletes ("
        "authname VARCHAR(32),"
        "delstatus INTEGER DEFAULT 0,"
        "endpoint INTEGER,"
        "created DATETIME,"
        "FOREIGN KEY (endpoint) REFERENCES Endpoints(endpoint) "
            "ON DELETE CASCADE);",

    // the "system" speed dialing is system group 10-99
    // per extension group personal speed dials 01-09 (#1-#9)

    "CREATE TABLE Speeds ("
        "authname VARCHAR(32),"                     // speed dial for...
        "target VARCHAR(128),"                  // local or external uri
        "extnbr INTEGER,"                       // speed dial #
        "CONSTRAINT dialing PRIMARY KEY (authname, extnbr),"
        "FOREIGN KEY (authname) REFERENCES Authorize(authname) "
            "ON DELETE CASCADE);",

    "CREATE TABLE Calling ("
        "authname VARCHAR(32),"                     // group hunt is part of
        "extnbr INTEGER,"                       // extension # to ring
        "extpriority INTEGER DEFAULT 0,"           // hunt group priority order
        "FOREIGN KEY (authname) REFERENCES Authorize(authname) "
            "ON DELETE CASCADE,"
        "FOREIGN KEY (extnbr) REFERENCES Extensions(extnbr) "
            "ON DELETE CASCADE);",

    "CREATE TABLE Admin ("
        "authname VARCHAR(32),"                     // group permission is for
        "extnbr INTEGER,"                       // extension with permission
        "FOREIGN KEY (authname) REFERENCES Authorize(authname) "
            "ON DELETE CASCADE,"
        "FOREIGN KEY (extnbr) REFERENCES Extensions(extnbr) "
            "ON DELETE CASCADE);",

    "CREATE TABLE Groups ("
        "grpnbr INTEGER,"                       // group tied to
        "extnbr INTEGER,"                       // group member extension
        "extpriority INTEGER DEFAULT -1,"          // coverage priority
        "CONSTRAINT Grouping PRIMARY KEY (grpnbr, extnbr),"
        "FOREIGN KEY (grpnbr) REFERENCES Extensions(extnbr) "
            "ON DELETE CASCADE,"
        "FOREIGN KEY (extnbr) REFERENCES Extensions(extnbr) "
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
        "msgseq INTEGER,"                       // helps exclude dups on devices
        "msgfrom VARCHAR(64),"                  // origin uri direct reply
        "msgto VARCHAR(64),"                    // to for creating uri
        "subject VARCHAR(80),"                  // subject header used
        "display VARCHAR(64),"                  // origin display name
        "posted TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "expires TIMESTAMP,"                    // estimated expiration
        "msgtype VARCHAR(8),"
        "msgtext TEXT);",

    "CREATE TABLE Outboxes ("
        "mid INTEGER,"
        "endpoint INTEGER,"
        "msgstatus INTEGER DEFAULT 0,"          // status code from stack
        "CONSTRAINT outbox PRIMARY KEY (endpoint, mid),"
        "FOREIGN KEY (mid) REFERENCES Messages(mid) "
            "ON DELETE CASCADE,"
        "FOREIGN KEY (endpoint) REFERENCES Endpoints(endpoint) "
            "ON DELETE CASCADE);",
};

static QStringList sqlitePragmas = {
    "PRAGMA locking_mode = EXCLUSIVE;",
    "PRAGMA synchronous = OFF;",
    "PRAGMA temp_store = MEMORY;",
};

#ifdef PRELOAD_DATABASE             // TODO: remove on ipl turnover
static QStringList sqlitePreload = {
    // "test1" and "test2", for extensions 101 and 102, password is "testing"
    "INSERT INTO Authorize(authname, authdigest, realm, secret, fullname, authtype, authaccess) "
        "VALUES('test1','MD5','testing','74d0a5bd38ed78708aacb9f056e40120','Test User #1','USER','LOCAL');",
    "INSERT INTO Authorize(authname, authdigest, realm, secret, fullname, authtype, authaccess, email) "
        "VALUES('test2','MD5','testing','6d292c665b1ed72b8bfdbb5d45173d98','Test User #2','USER','REMOTE','test2@localhost');",
    "INSERT INTO Authorize(authname, fullname, authtype, authaccess) "
        "VALUES('test', 'Test Group', 'GROUP', 'LOCAL');",
    "INSERT INTO Extensions(extnbr, authname) VALUES(100, 'test');",
    "INSERT INTO Extensions(extnbr, authname) VALUES(101, 'test1');",
    "INSERT INTO Extensions(extnbr, authname) VALUES(102, 'test2');",
    "INSERT INTO Endpoints(extnbr) VALUES(101);",
    "INSERT INTO Endpoints(extnbr) VALUES(102);",
    "INSERT INTO Groups(grpnbr, extnbr) VALUES(100, 101);",
    "INSERT INTO Groups(grpnbr, extnbr) VALUES(100, 102);",

    // 101 sysop and group admin for testing
    "INSERT INTO Admin(authname, extnbr) VALUES('system', 101);",
    "INSERT INTO Admin(authname, extnbr) VALUES('test', 101);",
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
