/*
 * Copyright (C) 2017-2018 Tycho Softworks.
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
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonObject>
#include "../Common/crypto.hpp"

namespace {
#ifdef DESKTOP_PREFIX
    QString storagePath(const QString& dbName)
    {
        return QString(DESKTOP_PREFIX) + "/" + dbName + ".db";
    }
#else
    QString storagePath(const QString& dbName)
    {
        auto path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if(path.isEmpty())
            return dbName + ".db";

        return path + "/" + dbName + ".db";
    }
#endif
}
//#if defined(DESKTOP_PREFIX)
//#define CONFIG_FROM DESKTOP_PREFIX "/settings.cfg", QSettings::IniFormat
//#endif

Storage *Storage::Instance = nullptr;
UString Storage::FromAddress;
UString Storage::ServerAddress;

Storage::Storage(const QString& dbName, const QString& key, const QVariantHash &cred)
{
    Q_UNUSED(key);

    Q_ASSERT(Instance == nullptr);
    Instance = this;

    existing = exists(dbName);

    db = QSqlDatabase::addDatabase("QSQLITE", "default");
    if(!db.isValid())
        return;

    db.setDatabaseName(storagePath(dbName));
    db.open();
    if(!db.isOpen())
        return;

    runQueries({
         "PRAGMA locking_mode = EXCLUSIVE;",
         "PRAGMA synchronous = OFF;",
         "PRAGMA temp_store = MEMORY;",
    });

    if(existing) {
        updateCredentials(cred);
        return;
    }

    runQueries({

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
             "privkey BLOB DEFAULT NULL,"           // extensions private key
             "pubkey BLOB DEFAULT NULL,"            // exensions public key
             "rootkey BLOB DEFAULT NULL,"           // devices private key
             "devkey BLOB DEFAULT NULL,"            // devices public key
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
            "mailto VARCHAR(128) DEFAULT '',"
            "puburi VARCHAR(128) DEFAULT '',"
            "sync DATETIME DEFAULT 0,"              // used for deletion sync
            "last DATETIME DEFAULT 0,"
            "info TEXT DEFAULT '',"
            "answer INTEGER DEFAULT 0,"             // auto answer this contact
            "topic VARCHAR(64) DEFAULT 'None',"       // current topic in session
            "tsync DATETIME DEFAULT 0,"              // when topic last changed
            "tseq INTEGER DEFAULT 0,"               // sequence of topic change
            "pubkey BLOB DEFAULT NULL,"
            "verify INTEGER DEFAULT 0,"             // key verification state
            "coverage INTEGER DEFAULT -1,"
            "answering INTEGER DEFAULT 0);",
        "CREATE INDEX ByContact ON Contacts(last DESC, sequence DESC);",

        // Messages are stored in the database to be reloaded when the client
        // restarts, and may be expired by date, but are not modified.  In
        // storage inbox vs outbox determined by whether sid == from or to.

        "CREATE TABLE Messages ("
            "msgfrom INTEGER,"                      // origin contact uid
            "msgto INTEGER,"                        // target contact uid
            "sid INTEGER,"                          // uid of session
            "subject VARCHAR(80),"                  // msg subject
            "reply VARCHAR(64) DEFAULT NULL,"       // reply uri if external
            "seqid INTEGER,"                        // used for unique timestamps
            "posted TIMESTAMP,"
            "callreason VARCHAR(8) DEFAULT NULL,"   // result of call
            "callduration INTEGER DEFAULT 0,"       // call duration...
            "expires TIMESTAMP ,"                   // carried expires header
            "status Integer Default 0,"             // add status of message 0 for active 1 for expired
            "msgtype VARCHAR(8),"
            "msgtext TEXT,"                         // depends on type...
            "msgjson TEXT DEFAULT NULL,"            // for json control docs
            "msgdata BLOB DEFAULT NULL,"            // attached binary
            "CONSTRAINT byDate PRIMARY KEY (sid, posted DESC, seqid DESC),"
            "FOREIGN KEY (sid) REFERENCES Contacts(uid) ON DELETE CASCADE);",

        "CREATE TABLE Devices ("
            "devlabel VARCHAR(32) PRIMARY KEY,"     // label of endpoint
            "devkey BLOB DEFAULT NULL,"             // public key to match
            "devagent VARCHAR(64),"                 // to determine type
            "devstatus INTEGER DEFAULT 0);",        // verification status
    });

    FromAddress = UString::uri(cred["schema"].toString(), cred["extension"].toString(), cred["host"].toString().toUtf8(), static_cast<quint16>(cred["port"].toInt()));
    ServerAddress = UString::uri(cred["schema"].toString(), cred["host"].toString().toUtf8(), static_cast<quint16>(cred["port"].toInt()));
    runQuery("INSERT INTO Credentials(extension, user, display, label, secret, server, schema, host, port, type, realm) VALUES(?,?,?,?,?,?,?,?,?,?,?);",
        {cred["extension"], cred["user"], cred["display"], cred["label"], cred["secret"], QString::fromUtf8(ServerAddress), cred["schema"], cred["host"], cred["port"], cred["type"], cred["realm"]});
    if(cred["rootkey"].isNull() || cred["devkey"].isNull()) {
        QVariantHash tmp;
        createKeys(tmp);
    }
    else
        runQuery("UPDATE Credentials SET devkey=?, rootkey=? WHERE id=1;", {cred["devkey"], cred["rootkey"]});
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

QString Storage::name(const QVariantHash& creds, const UString &id)
{
    UString extension = creds["extension"].toString().toUtf8();
    UString label = creds["label"].toString().toLower().toUtf8();
    UString key = extension + ":" + label + ":" + id;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(key);
    return hash.result().toHex();
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

int Storage::runQueries(const QStringList& list)
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

QVariant Storage::insert(const QString& request, const QVariantList &parms)
{
    if(!db.isOpen())
        return QVariant();

    QSqlQuery query(db);
    query.prepare(request);
    int count = -1;
    qDebug() << "Request " << request << " LIST " << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));

    if(!query.exec()) {
        qWarning() << "Query failed; " << query.lastQuery();
        return QVariant();
    }

    return query.lastInsertId();
}

void Storage::initialLogin(const QString& dbName, const QString& key, QVariantHash& creds)
{
    if(exists(dbName)) {
        creds["initialize"] = "user";
        Storage tmp(dbName, key, creds);
        if(!tmp.isActive())
            return;
        auto keys = tmp.getRecord("SELECT rootkey,devkey FROM credentials");
        creds["devkey"] = keys["devkey"];
        creds["rootkey"] = keys["rootkey"];
    }
    if(creds["rootkey"].isNull() || creds["devkey"].isNull()) {
        auto pair = Crypto::keypair();
        creds["devkey"] = pair.first;
        creds["rootkey"] = pair.second;
        qDebug() << "Initial device key created";
    }
}

void Storage::verifyDevice(const QString& label, const QByteArray& key, const QString& agent)
{
    runQuery("DELETE FROM Devices WHERE (devlabel=?);", {label});
    runQuery("INSERT INTO Devices (devlabel, devkey, devagent,devstatus) VALUES(?,?,?,1);",
        {label, key, agent});
}

void Storage::verifyDevices(QJsonArray& devices)
{
    QHash<QString,QJsonObject> labels;
    foreach(auto device, devices) {
        auto json = device.toObject();
        auto label = json["u"].toString();
        json["v"] = 0;
        labels[label] = json;
    }

    auto query = getRecords("SELECT * From Devices;");
    while(query.isActive() && query.next()) {
        auto record = query.record();
        auto label = record.value("devlabel").toString();
        auto status = record.value("devstatus").toInt();
        auto json = labels[label];
        if(json.isEmpty()) {
            runQuery("DELETE FROM Devices WHERE (devlabel=?);", {label});
            continue;
        }

        auto lkey = record.value("devkey").toByteArray();
        auto rkey = QByteArray::fromHex(json["k"].toString().toUtf8());
        if(rkey.count() < 1)
            continue;

        // once verification fails, is marked until verify or removal
        if(status == 2 || lkey != rkey)
            json["v"] = 2;
        else
            json["v"] = 1;

        runQuery("UPDATE Devices SET devstatus=? WHERE (devlabel=?);",
            {json["v"].toInt(), label});

        labels[label] = json;
    }

    auto index = labels.keys();
    index.sort(Qt::CaseInsensitive);
    QJsonArray result;
    foreach(auto label, index) {
        auto json = labels[label];
        if(!json.isEmpty())
            result << json;
    }
    devices = result;
}

void Storage::createKeys(QVariantHash& creds)
{
    if(creds["rootkey"].isNull() || creds["devkey"].isNull()) {
        auto pair = Crypto::keypair();
        runQuery("UPDATE Credentials SET devkey=?, rootkey=? WHERE id=1;", {pair.first, pair.second});
        creds["devkey"] = pair.first;
        creds["rootkey"] = pair.second;
        qDebug() << "Generated device key";
    }
}

void Storage::updateSelf(const QString& text)
{
    runQuery("UPDATE Credentials SET display=? WHERE id=1;", {text});
}

void Storage::updateCredentials(const QVariantHash &update)
{
    if(!db.isOpen() || update.isEmpty())
        return;

    auto cred = credentials();
    createKeys(cred);
    foreach(auto key, update.keys())
        cred[key] = update[key];

    FromAddress = UString::uri(cred["schema"].toString(), cred["extension"].toString(), cred["host"].toString().toUtf8(), static_cast<quint16>(cred["port"].toInt()));
    ServerAddress = UString::uri(cred["schema"].toString(), cred["host"].toString().toUtf8(), static_cast<quint16>(cred["port"].toInt()));
    runQuery("UPDATE Credentials SET server=?, schema=?, host=?, port=?, user=?, secret=?, type=?, realm=? WHERE id=1;",
        {QString::fromUtf8(ServerAddress), cred["schema"], cred["host"], cred["port"], cred["user"], cred["secret"], cred["type"], cred["realm"]});
}

bool Storage::exists(const QString& dbName)
{
    if(dbName.isEmpty())
        return false;
    return QFileInfo::exists(storagePath(dbName));
}

void Storage::remove(const QString& dbName)
{
    QFile file(storagePath(dbName));
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


#include <QStandardPaths>
int Storage::copyDb(const QString &dbName){

    QVariantHash extlab = getRecord("SELECT extension, label from Credentials",{});
    auto ext = extlab["extension"].toString();
    auto lab = extlab["label"].toString();
    qDebug() << "Extension is " << ext << " and label is " << lab;
    QString backupfilename = ext + "-" + lab + "-" + "backup.db";
    qDebug() << "Backup filename " << backupfilename;

    FOR_DEBUG(
    QString fullpath = QString(DESKTOP_PREFIX) + "/" +backupfilename;
     if (QFile::copy(storagePath(dbName),(fullpath)))
         return 0;
     else
         return 1;
     )
    FOR_RELEASE(
        auto path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        QString fileName = backupfilename;
        QString fullPath;
        if(path.isEmpty()){
            fullPath = fileName;
        }
        else
            fullPath = path + "/" + fileName;
        if (QFile::copy(storagePath(dbName),fullPath))
            return 0;
        else
            return 1;
        )
}

bool Storage::importDb(const QString& dbName, const QVariantHash& creds)
{
    auto ext = creds["extension"].toString();
    auto lab = creds["label"].toString();
    qDebug() << "Extension is " << ext << " and label is " << lab << endl;
    QString backupfilename = ext + "-" + lab + "-" + "backup.db";

    FOR_DEBUG(
        QString fullpath = QString(DESKTOP_PREFIX) + "/" + backupfilename;
        qDebug() << fullpath;
        QFile::remove(storagePath(dbName));
        return QFile::copy((QString(DESKTOP_PREFIX) + "/" + backupfilename),storagePath(dbName));
    )

    FOR_RELEASE(
       auto path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
       QString fileName = backupfilename;
       QString fullPath;
       if(path.isEmpty()){
           fullPath = fileName;
       }
       else
           fullPath = path + "/" + fileName;
       return QFile::copy(fullPath,storagePath(dbName)))
}

