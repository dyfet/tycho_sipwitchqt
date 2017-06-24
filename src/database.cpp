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

#include "compiler.hpp"
#include "server.hpp"
#include "logging.hpp"
#include "main.hpp"
#include "sqldriver.hpp"
#include "database.hpp"

#include <QDate>
#include <QTime>
#include <QTimer>
#include <QSqlQuery>
#include <QSqlError>

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

Database::Database(unsigned order)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;

    moveToThread(Server::createThread("database", order));
    timer.moveToThread(thread());
    timer.setSingleShot(true);

    Server *server = Server::instance();
    connect(thread(), &QThread::finished, this, &QObject::deleteLater);
    connect(server, &Server::changeConfig, this, &Database::applyConfig);
    connect(&timer, &QTimer::timeout, this, &Database::onTimeout);
}

Database::~Database()
{
    Instance = nullptr;
    close();
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

bool Database::runQuery(const QString &request, QVariantList parms)
{
    if(!reopen())
        return false;

    QSqlQuery query(db);
    query.prepare(request);

    int count = -1;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));
        

    if(query.exec() != true) {
        Logging::notice() << "Query failed; " << query.lastError().text();
        return false;
    }
    return true;
}

QSqlRecord Database::getRecord(const QString& request, QVariantList parms)
{
    if(!reopen())
        return QSqlRecord();
    
    QSqlQuery query(db);
    query.prepare(request);
    int count = -1;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));

    if(!query.exec())
        return QSqlRecord();

    if(!query.next())
        return QSqlRecord();

    return query.record();
}

void Database::init(unsigned order)
{
    Q_ASSERT(Database::Instance == nullptr);
    new Database(order);
}

void Database::close()
{
    timer.stop();
    if(db.isOpen()) {
        db.close();
        qDebug() << "Database(CLOSE)";
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
            qDebug() << "Database(FAILED)";
        )
        FOR_RELEASE(
            Logging::err() << "Invalid database driver " << driver;
        )
        return false;
    }
    db.setDatabaseName(name);
    db.open();
    if(!db.isOpen()) {
        FOR_DEBUG(
            qDebug() << "Database(FAILED)";
        )
        FOR_RELEASE(
            Logging::err() << "Database failed; " << db.lastError().text();
        )
        return false;
    }

    runQuery(Util::pragmaQuery(driver));

    qDebug() << "Database(OPEN)";
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

    Logging::info() << "Loaded database driver " << driver;

    if(init) {
        runQuery(Util::createQuery(driver));
        runQuery("INSERT INTO Tycho_Switches(uuid, realm) VALUES (?,?);", {uuid, realm});
    }
    else if(Util::dbIsFile(driver))
        runQuery("VACUUM");

    if(!runQuery("UPDATE Tycho_Switches SET uuid=? WHERE realm=?;", {uuid, realm}))
        runQuery("INSERT INTO Tycho_Switches(uuid, realm) VALUES (?,?);", {uuid, realm});

    int count = getCount("Switches");
    if(!failed)
        emit countResults("swq", count);

    return true;
}

int Database::getCount(const QString& id)
{
    qDebug() << "Count records...";

    int count = 0;
    QSqlQuery query(db);
    query.prepare(QString("SELECT COUNT (*) FROM Tycho_") + id + ";");
    if(!query.exec()) {
        Logging::err() << "Extensions; error=" << query.lastError().text();
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
        if(opened)
            count = getCount("Extensions");
        if(!failed)
            emit countResults("ext", count);
        return true;
    }
    return true;
}

void Database::onTimeout()
{
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
    driver = config["database/driver"].toString();
    uuid = Server::sym(CURRENT_UUID);
    failed = false;

    if(realm.isEmpty()) {
        realm = Server::sym(CURRENT_NETWORK);
        if(realm.isEmpty() || realm == "local" || realm == "localhost" || realm == "localdomain")
            realm = Server::sym(CURRENT_UUID);
    }

    qDebug() << "DRIVER NAME " << driver;

    if(driver.isEmpty() && host.isEmpty())
        driver = "QSQLITE";
    else if(driver.isEmpty())
        driver = "QMYSQL";

    driver = driver.toUpper();
    if(Util::dbIsFile(driver))
        name = "local.db";
    else if(name.isEmpty())
        name = "REALM_" + realm;
        
    qDebug() << "*** Database realm" << realm;
    create();
}

void Database::countExtensions()
{
    Q_ASSERT(Instance != nullptr);
    QCoreApplication::postEvent(Instance,
        new DatabaseEvent(COUNT_EXTENSIONS));
}

