/**
 ** Copyright 2017 Tycho Softworks.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "sqldriver.hpp"

#include <QStringList>

static QStringList sqliteTables = {
    "CREATE TABLE Tycho_Switches ("
        "realm VARCHAR(128) PRIMARY KEY,"       // realm we use
        "digest VARCHAR(8) DEFAULT 'MD5',"      // digest type used
        "dialing CHAR(1) DEFAULT '3',"          // dialing plan used
        "uuid CHAR(36) NOT NULL);",             // switch uuid

    "CREATE TABLE Tycho_Extensions ("
        "number INTEGER PRIMARY KEY,"           // ext number
        "type VARCHAR(8) DEFAULT 'NONE',"       // ext type/suspended state
        "alias VARCHAR(32),"                    // authorization id/pub alias
        "digest VARCHAR(8) DEFAULT 'MD5',"      // digest type for account
        "secret VARCHAR(128),"                  // auth secret
        "display VARCHAR(64));",                // display name
    "CREATE INDEX TychoAuthorizedAliases ON Tycho_Extensions(alias) WHERE secret IS NOT NULL;",
};

static QStringList sqlitePragmas = {
    "PRAGMA locking_mode = EXCLUSIVE;",
    "PRAGMA synchronous = OFF;",
    "PRAGMA temp_store = MEMORY;"
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
