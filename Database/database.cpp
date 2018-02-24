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

#include "../Common/compiler.hpp"
#include "../Server/server.hpp"
#include "../Server/output.hpp"
#include "../Server/manager.hpp"
#include "../Server/main.hpp"
#include "sqldriver.hpp"
#include "database.hpp"

#include <QDate>
#include <QTime>
#include <QTimer>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

enum {
    // database events...
    COUNT_EXTENSIONS = QEvent::User + 1,
};

class DatabaseEvent final : public QEvent
{
    Q_DISABLE_COPY(DatabaseEvent)
public:
    DatabaseEvent(int event, Request *req = nullptr) :
    QEvent(static_cast<QEvent::Type>(event)), request(req) {}

    ~DatabaseEvent();

    Request *reply() const {
        return request;
    }

private:
    Request *request;
};

DatabaseEvent::~DatabaseEvent() {}

static bool failed = false;

Database *Database::Instance = nullptr;

Database::Database(unsigned order) :
QObject()
{
    firstNumber = lastNumber = -1;
    dbSequence = 0;

    expiresNat = 80;
    expiresUdp = 300;
    expiresTcp = 600;

    moveToThread(Server::createThread("database", order));
    timer.moveToThread(thread());
    timer.setSingleShot(true);

    Server *server = Server::instance();
    connect(thread(), &QThread::finished, this, &QObject::deleteLater);
    connect(server, &Server::changeConfig, this, &Database::applyConfig);
    connect(&timer, &QTimer::timeout, this, &Database::onTimeout);

    Manager *manager = Manager::instance();
    connect(manager, &Manager::sendRoster, this, &Database::sendRoster);
}

Database::~Database()
{
    Instance = nullptr;
    close();
}

QVariantHash Database::result(const QSqlRecord& record)
{
    QVariantHash hash;
    int pos = 0;
    while(pos < record.count()) {
        hash[record.fieldName(pos)] = record.value(pos);
        ++pos;
    }
    return hash;
}

int Database::runQuery(const QStringList& list)
{
    int count = 0;

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

bool Database::runQuery(const QString &request, const QVariantList &parms)
{
    if(!reopen())
        return false;

    QSqlQuery query(db);
    query.prepare(request);

    int count = -1;
        qDebug() << "Query" << request << "LIST" << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));
        
    if(query.exec() != true) {
        warning() << "Query failed; " << query.lastError().text() << " for " << query.lastQuery();
        return false;
    }
    return true;
}

QSqlRecord Database::getRecord(const QString& request, const QVariantList &parms)
{
    if(!reopen())
        return QSqlRecord();
    
    QSqlQuery query(db);
    query.prepare(request);
    int count = -1;
    qDebug() << "Request " << request << " LIST " << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));

    if(!query.exec())
        return QSqlRecord();

    if(!query.next())
        return QSqlRecord();

    return query.record();
}

QSqlQuery Database::getRecords(const QString& request, const QVariantList &parms)
{
    if(!reopen())
        return QSqlQuery();

    QSqlQuery query(db);
    query.prepare(request);
    int count = -1;
    qDebug() << "Query " << request << " LIST " << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));

    if(!query.exec())
        return QSqlQuery();

    return query;
}

void Database::init(unsigned order)
{
    Q_ASSERT(Instance == nullptr);
    Instance = new Database(order);
}

void Database::close()
{
    timer.stop();
    if(db.isOpen()) {
        db.close();
        debug() << "Database(CLOSE)";
    }
    if(db.isValid()) {
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase("default");
    }
    failed = false;
}

bool Database::reopen()
{
    if(failed)
        return false;

    if(db.isOpen()) {
        timer.setInterval(interval);
        timer.setSingleShot(true);
        return true;
    }

    close();
    db = QSqlDatabase::addDatabase(driver, "default");
    if(!db.isValid()) {
        failed = true;
        FOR_DEBUG(
            debug() << "Database(FAILED)";
        )
        FOR_RELEASE(
            error() << "Invalid database driver " << driver;
        )
        return false;
    }
    db.setDatabaseName(name);
    if(!isFile()) {
        if(!host.isEmpty())
            db.setHostName(host);
        if(port)
            db.setPort(port);
        if(!user.isEmpty())
            db.setUserName(user);
        if(!pass.isEmpty())
            db.setPassword(pass);
    }
    db.open();
    if(!db.isOpen()) {
        FOR_DEBUG(
            debug() << "Database(FAILED)";
        )
        FOR_RELEASE(
            error() << "Database failed; " << db.lastError().text();
        )
        return false;
    }

    runQuery(Util::pragmaQuery(driver));

    debug() << "Database(OPEN)";
    timer.setInterval(interval);
    timer.start();
    return true;
}

bool Database::create()
{
    bool init = Util::dbIsFile(driver);

    failed = false;

    if(init && QDir::current().exists(name))
        init = false;

    if(!reopen())
        return false;

    info() << "Loaded database driver " << driver;

    if(init) {
        runQuery(Util::createQuery(driver));
        runQuery("INSERT INTO Config(realm) VALUES(?);", {
                     realm});
        runQuery("INSERT INTO Switches(uuid, version) VALUES (?,?);", {
                     uuid, PROJECT_VERSION});
        runQuery("INSERT INTO Authorize(name, type, access) VALUES(?,?,?);", {
                     "system", "SYSTEM", "LOCAL"});
        runQuery("INSERT INTO Authorize(name, type, access) VALUES(?,?,?);", {
                     "nobody", "SYSTEM", "LOCAL"});
        runQuery("INSERT INTO Authorize(name, type, access) VALUES(?,?,?);", {
                     "anonymous", "SYSTEM", "DISABLED"});
        runQuery("INSERT INTO Authorize(name, type, access) VALUES(?,?,?);", {
                     "operators", "SYSTEM", "PILOT"});
        runQuery("INSERT INTO Extensions(number, name, display) VALUES (?,?,?);", {
                     0, "operators", "Operators"});
        runQuery(Util::preloadConfig(driver));
    }
    else if(Util::dbIsFile(driver)) {
        runQuery("UPDATE Config SET realm=? WHERE id=1;", {realm});
        runQuery("VACUUM");
    }

    QSqlQuery query(db);
    query.prepare("SELECT realm, series, dialing FROM Config WHERE id=1;");
    if(!query.exec()) {
        error() << "No config found " << driver;
        close();
        return false;
    }
    else if(query.next())
        config = query.record();

    if(config.value("dialing").toString() == "STD3") {
        firstNumber = 100;
        lastNumber = 699;
    }

    qDebug() << "Extension range" << firstNumber << "to" << lastNumber;

    if(!runQuery("UPDATE Switches SET version=? WHERE uuid=?;", {PROJECT_VERSION, uuid}))
        runQuery("INSERT INTO Switches(uuid, version) VALUES (?,?);", {uuid, PROJECT_VERSION});

    return true;
}

int Database::getCount(const QString& id)
{
    qDebug() << "Count records...";

    int count = 0;
    QSqlQuery query(db);
    query.prepare(QString("SELECT COUNT (*) FROM ") + id + ";");
    if(!query.exec()) {
        error() << "Extensions; error=" << query.lastError().text();
        failed = true;
    }
    else if(query.next()) {
        count = query.value(0).toInt();
    }
    return count;
}

bool Database::event(QEvent *evt)
{
    int id = static_cast<int>(evt->type());
    if(id < QEvent::User + 1)
        return QObject::event(evt);

    auto reply = (static_cast<DatabaseEvent *>(evt))->reply();

    // reply->value("key"), etc, to pass parms to query here!!!!

    bool opened = reopen();

    if(!opened && reply) {
        reply->notifyFailed();
        return true;
    }

    int count = 0;

    switch(id) {
    case COUNT_EXTENSIONS:
        return true;
    }
    return true;
}

void Database::sendRoster(const Event& event)
{
    qDebug() << "Seeking roster for" << event.number();

    auto query = getRecords("SELECT * FROM Extensions,Authorize WHERE Extensions.name = Authorize.name ORDER BY Extensions.number");

    QJsonArray list;
    qDebug() << "REQUEST" << event.request();
    while(query.isActive() && query.next()) {
        auto record = query.record();
        auto display = record.value("display").toString();
        auto dialing = record.value("number").toString();
        if(display.isEmpty())
            display = record.value("fullname").toString();
        if(display.isEmpty())
            display = record.value("name").toString();

        UString uri = event.uriTo(dialing);
        QJsonObject profile {
            {"a", record.value("name").toString()},
            {"n", record.value("number").toInt()},
            {"u", QString::fromUtf8(uri)},
            {"d", display},
            {"t", record.value("type").toString()},
        };

        list << profile;
    }
    QJsonDocument jdoc(list);
    auto json = jdoc.toJson(QJsonDocument::Compact);
    Context::roster(event, json);
}

void Database::onTimeout()
{
    if(isFile())
        close();
}

void Database::applyConfig(const QVariantHash& config)
{
    realm = config["realm"].toString();
    name = config["database/name"].toString();
    host = config["database/host"].toString();
    port = config["database/port"].toInt();
    user = config["database/username"].toString();
    pass = config["database/password"].toString();
    driver = config["database"].toString();
    uuid = Server::uuid();
    failed = false;

    if(realm.isEmpty()) {
        realm = Server::sym(CURRENT_NETWORK);
        if(realm.isEmpty() || realm == "local" || realm == "localhost" || realm == "localdomain")
            realm = Server::uuid();
    }

    qDebug() << "DRIVER NAME " << driver;

    if(driver.isEmpty() && host.isEmpty())
        driver = "QSQLITE";
    else if(driver.isEmpty())
        driver = "QMYSQL";

    // normalize names for Qt database driver plugins
    driver = driver.toUpper();
    if(driver[0] != QChar('Q'))
        driver = "Q" + driver;

    if(driver == "QSQLITE3")
        driver = "QSQLITE";

    if(Util::dbIsFile(driver))
        name = "local.db";
    else if(name.isEmpty())
        name = "REALM_" + realm.toUpper();

    create();
    emit updateAuthorize(config, db.isOpen());
}

void Database::countExtensions()
{
    Q_ASSERT(Instance != nullptr);
    QCoreApplication::postEvent(Instance,
        new DatabaseEvent(COUNT_EXTENSIONS));
}

