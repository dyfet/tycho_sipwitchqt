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

#include "storage.hpp"
#include <QFileInfo>
#include <QDebug>

#ifdef DESKTOP_PREFIX

static QString storagePath()
{
    return QString(DESKTOP_PREFIX) + "/client.db";
}

#else

#include <QStandardPaths>

static QString storagePath()
{
	QString fileName = "sipwitchqt-client.db";
    auto path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if(path.isEmpty())
        return fileName;

    return path + "/" + fileName;
}

#endif

Storage *Storage::Instance = nullptr;
UString Storage::FromAddress;
UString Storage::ServerAddress;

Storage::Storage(const QString& key, const QVariantHash &cred)
{
    Q_UNUSED(key);

    Q_ASSERT(Instance == nullptr);
    Instance = this;

    bool existing = exists();
    
    db = QSqlDatabase::addDatabase("QSQLITE", "default");
    if(!db.isValid())
        return;

    db.setDatabaseName(storagePath());
    db.open();
    if(!db.isOpen())
        return;

    runQuery({
         "PRAGMA locking_mode = EXCLUSIVE;",
         "PRAGMA synchronous = OFF;",
         "PRAGMA temp_store = MEMORY;",
    });

    if(existing) {
        updateCredentials(cred);
        return;
    }

    runQuery({

        // This is a master config table.

        "CREATE TABLE Credentials ("
             "id INTEGER PRIMARY KEY,"              // rowid in sqlite
             "extension INTEGER,"                   // extension #
             "user VARCHAR(64) NOT NULL,"           // auth userid
             "display VARCHAR(64) NOT NULL,"        // display name
             "label VARCHAR(32) NOT NULL,"          // endpoint label
             "secret VARCHAR(64) NOT NULL,"         // auth secret
             "schema VARCHAR(8) NOT NULL,"          // server schema
             "server VARCHAR(64) NOT NULL,"         // server address
             "host VARCHAR(64) NOT NULL,"           // server we use
             "port INTEGER DEFAULT 0,"              // server port
             "realm VARCHAR(128) NOT NULL,"         // remote realm
             "type VARCHAR(16) NOT NULL,"           // algorithm used
             "series INTEGER DEFAULT 9);",          // site db series

        // Contacts generally do not delete, but they are modified with
        // roster updates.  we may add roster deleting in the future though.
        // Contacts and "sessions" are somewhat synomonous in the database.

        "CREATE TABLE Contacts ("
            "uid INTEGER PRIMARY KEY AUTOINCREMENT,"
            "extension INTEGER DEFAULT -1,"
            "sequence INTEGER DEFAULT 0,"           // helps with timestamp ordering
            "type VARCHAR(8) DEFAULT 'USER',"
            "user VARCHAR(64),"                     // authorizing user
            "uri VARCHAR(128),"
            "dialing VARCHAR(64),"                  // used to tie together msgs
            "display VARCHAR(64) DEFAULT NULL,"
            "last DATETIME DEFAULT 0);",
        "CREATE INDEX ByContact ON Contacts(last, sequence DESC);",

        // Messages are stored in the database to be reloaded when the client
        // restarts, and may be expired by date, but are not modified.  In
        // storage inbox vs outbox determined by whether sid == from or to.

        "CREATE TABLE Messages ("
            "msgfrom INTEGER,"                      // origin contact uid
            "msgto INTEGER,"                        // target contact uid
            "sid INTEGER,"                          // uid of session
            "subject VARCHAR(80),"                  // msg subject
            "display VARCHAR(64),"                  // who its from as shown
            "reply VARCHAR(64) DEFAULT NULL,"       // reply uri if external
            "seqid INTEGER,"                        // used for unique timestamps
            "posted TIMESTAMP,"
            "callreason VARCHAR(8) DEFAULT NULL,"   // result of call
            "callduration INTEGER DEFAULT 0,"       // call duration...
            "expires INTEGER DEFAULT 0,"            // carried expires header
            "msgtype VARCHAR(8),"
            "msgtext TEXT,"                         // depends on type...
            "FOREIGN KEY (sid) REFERENCES Contacts(uid));",
        "CREATE UNIQUE INDEX ByDate ON Messages(sid, posted, seqid);",
    });

    FromAddress = UString::uri(cred["schema"].toString(), cred["extension"].toString(), cred["host"].toString().toUtf8(), static_cast<quint16>(cred["port"].toInt()));
    ServerAddress = UString::address(cred["schema"].toString(), cred["host"].toString().toUtf8(), static_cast<quint16>(cred["port"].toInt()));
    runQuery("INSERT INTO Credentials(extension, user, display, label, secret, server, schema, host, port, type, realm) VALUES(?,?,?,?,?,?,?,?,?,?,?);",
        {cred["extension"], cred["user"], cred["display"], cred["label"], cred["secret"], QString::fromUtf8(ServerAddress), cred["schema"], cred["host"], cred["port"], cred["type"], cred["realm"]});
}

Storage::~Storage()
{
    Instance = nullptr;
    if(db.isOpen()) {
        db.close();
    }
    if(db.isValid()) {
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase("default");
    }
}

QSqlQuery Storage::getQuery(const QString& request, const QVariantList& parms)
{
    if(!db.isOpen())
        return QSqlQuery();

    QSqlQuery query(db);
    query.prepare(request);
    int count = -1;
    qDebug() << "Request " << request << " LIST " << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));
    if(!query.exec())
        return QSqlQuery();

    return query;
}

QVariantHash Storage::getRecord(const QString& request, const QVariantList& parms)
{
    if(!db.isOpen())
        return QVariantHash();

    QSqlQuery query(db);
    query.prepare(request);
    int count = -1;
    qDebug() << "Request " << request << " LIST " << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));

    if(!query.exec())
        return QVariantHash();

    return next(query);
}

QSqlQuery Storage::getRecords(const QString& request, const QVariantList& parms)
{
    QVariantHash item;

    if(!db.isOpen())
        return QSqlQuery();

    QSqlQuery query(db);
    query.prepare(request);
    int count = -1;
    qDebug() << "Request " << request << " LIST " << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));

    if(!query.exec())
        return QSqlQuery();

    return query;
}

int Storage::runQuery(const QStringList& list)
{
    int count = 0;

    if(!db.isOpen())
        return 0;

    if(list.count() < 1)
        return 0;

    foreach(auto request, list) {
        if(!runQuery(request))
            break;
        ++count;
    }
    qDebug() << "Performed" << count << "of" << list.count() << "queries";
    return count;
}

bool Storage::runQuery(const QString &request, const QVariantList& parms)
{
    if(!db.isOpen())
        return false;

    QSqlQuery query(db);
    query.prepare(request);

    int count = -1;
        qDebug() << "Query" << request << "LIST" << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));

    if(!query.exec()) {
        qWarning() << "Query failed; " << query.lastQuery();
        return false;
    }
    return true;
}

void Storage::updateCredentials(const QVariantHash &update)
{
    if(!db.isOpen() || update.isEmpty())
        return;

    auto cred = credentials();
    foreach(auto key, update.keys())
        cred[key] = update[key];

    FromAddress = UString::uri(cred["schema"].toString(), cred["extension"].toString(), cred["host"].toString().toUtf8(), static_cast<quint16>(cred["port"].toInt()));
    ServerAddress = UString::address(cred["schema"].toString(), cred["host"].toString().toUtf8(), static_cast<quint16>(cred["port"].toInt()));
    runQuery("UPDATE Credentials SET server=?, schema=?, host=?, port=?, user=?, secret=?, type=?, realm=? WHERE id=1;",
        {QString::fromUtf8(ServerAddress), cred["schema"], cred["host"], cred["port"], cred["user"], cred["secret"], cred["type"], cred["realm"]});
}

bool Storage::exists()
{
	return QFileInfo::exists(storagePath());
}

void Storage::remove()
{
	QFile file(storagePath());
	file.remove();
}

QVariantHash Storage::next(QSqlQuery& query)
{
    if(!query.next())
        return QVariantHash();

    QVariantHash item;
    auto current = query.record();
    int pos = 0;
    while(pos < current.count()) {
        item[current.fieldName(pos)] = current.value(pos);
        ++pos;
    }
    return item;
}

