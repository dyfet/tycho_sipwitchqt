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
#include <QStandardPaths>
#include <QFileInfo>
#include <QFile>
#include <QSqlQuery>
#include <QDebug>

#ifdef DESKTOP_PREFIX

static QString storagePath()
{
    return QString(DESKTOP_PREFIX) + "/client.db";
}

#else

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

Storage::Storage(const QString& key, QHash<QString, QString> cred)
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
        "CREATE TABLE Credentials ("
             "id INTEGER PRIMARY KEY,"              // rowid in sqlite
             "user VARCHAR(64) NOT NULL,"           // auth userid
             "secret VARCHAR(64) NOT NULL,"         // auth secret
             "server VARCHAR(64) NOT NULL,"         // server we use
             "port INTEGER DEFAULT 0,"              // server port
             "realm VARCHAR(128) NOT NULL,"         // remote realm
             "type VARCHAR(16) NOT NULL,"           // algorithm used
             "series INTEGER DEFAULT 9);",          // site db series

        "CREATE TABLE Contacts ("
            "extension INTEGER DEFAULT 0,"
            "uri VARCHAR(128),"
            "display VARCHAR(64),"
            "created TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "last DATETIME DEFAULT 0,"
            "PRIMARY KEY (uri));",
        "CREATE UNIQUE INDEX Listing ON Contacts(last, display) WHERE last > 0;",
        "CREATE UNIQUE INDEX Extensions ON Contacts(extension) WHERE extension > 0;",
    });

    runQuery("INSERT INTO Credentials(user, secret, server, port, type, realm) VALUES(?,?,?,?,?,?);",
        {cred["user"], cred["secret"], cred["server"], cred["port"], cred["type"], cred["realm"]});
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
    qDebug() << "***** REQUEST " << request << " LIST " << parms;
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
    qDebug() << "***** REQUEST " << request << " LIST " << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));

    if(!query.exec())
        return QVariantHash();

    return next(query);
}

QList<QVariantHash> Storage::getRecords(const QString& request, const QVariantList& parms)
{
    QList<QVariantHash> list;
    QVariantHash item;

    if(!db.isOpen())
        return list;

    QSqlQuery query(db);
    query.prepare(request);
    int count = -1;
    qDebug() << "***** REQUEST " << request << " LIST " << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));

    if(!query.exec())
        return list;

    while(query.next()) {
        auto current = query.record();
        int pos = 0;
        while(pos < current.count()) {
            item[current.fieldName(pos)] = current.value(pos);
            ++pos;
        }
        list << item;
    }

    return list;
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

    if(query.exec() != true) {
        qWarning() << "Query failed; " << query.lastQuery();
        return false;
    }
    return true;
}

void Storage::updateCredentials(const QHash<QString,QString> &update)
{
    if(!db.isOpen() || update.isEmpty())
        return;

    auto cred = credentials();
    foreach(auto key, update.keys())
        cred[key] = update[key];
    runQuery("UPDATE Credentials SET user=?, secret=?, server=?, port=?, type=?, realm=? WHERE id=1;",
        {cred["user"], cred["secret"], cred["server"], cred["port"], cred["type"], cred["realm"]});
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

