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
#include "authorize.hpp"
#include <QSqlError>

Authorize *Authorize::Instance = nullptr;

Authorize::Authorize(unsigned order) :
QObject(), db(nullptr)
{
    database = Database::instance();
    if(order) {
        auto thread = Server::createThread("authorize", order);
        moveToThread(thread);
    }
    else
        moveToThread(database->thread());

    connect(thread(), &QThread::finished, this, &QObject::deleteLater);
    connect(database, &Database::updateAuthorize, this, &Authorize::activate);
    connect(this, &Authorize::copyOutboxes, database, &Database::copyOutbox);

    // future connections for quick aync between manager and auth
    Manager *manager = Manager::instance();
    connect(manager, &Manager::findEndpoint, this, &Authorize::findEndpoint);
    connect(this, &Authorize::createEndpoint, manager, &Manager::createRegistration);
}

Authorize::~Authorize()
{
    if(local.isValid() && local.isOpen()) {
        local.close();
        local = QSqlDatabase();
        QSqlDatabase::removeDatabase("auth");
    }
    Instance = nullptr;
}

void Authorize::init(unsigned order)
{
    Q_ASSERT(Instance == nullptr);
    Instance = new Authorize(order);
}

// This finds an endpoint to be authorized.  It can do many db exclusions,
// but does not validate the secret, as that happens in manager using the
// registry object.  This is because the registry object caches db info and
// we only query for initial authorization, not during existing refresh.
void Authorize::findEndpoint(const Event& event)
{
    qDebug() << "Seeking endpoint" << event.number();
    //emit createEndpoint(event, QVariantHash());
    //Context::reply(event, SIP_NOT_FOUND);

    if(database->firstNumber < 1 || db == nullptr) {
        Context::reply(event, SIP_INTERNAL_SERVER_ERROR);
        return;
    }

    auto number = event.number();
    auto label = event.label();
    auto expires = event.expires();

    // if a registration release and we got here, we just ignore...
    if(expires < 1) {
        Context::reply(event, SIP_OK);
        return;
    }

    if(number < database->firstNumber || number > database->lastNumber) {
        warning() << "Invalid extension " << number << " for authorization";
        Context::reply(event, SIP_DOES_NOT_EXIST_ANYWHERE);
        return;
    }

    auto endpoint = getRecord("SELECT * FROM Endpoints WHERE (extnbr=?) AND (label=?)", {number, label});
    auto extension = getRecord("SELECT * FROM Extensions WHERE extnbr=?", {number});

    if(extension.count() < 1) {
        warning() << "Cannot authorize " << number << "; not found";
        Context::reply(event, SIP_NOT_FOUND);
        return;
    }

    // a sipwitch client specific registration feature...
    if(event.initialize() == "label") {
        if(endpoint.count() < 1) {
            warning() << "Initializing database for " << number << " with label " << label;
            runQuery("INSERT INTO Endpoints(extnbr, label) VALUES (?,?);", {number, label});
            endpoint = getRecord("SELECT * FROM Endpoints WHERE (extnbr=?) AND (label=?)", {number, label});
            auto origin = getRecord("SELECT * FROM Endpoints WHERE (extnbr=?) AND (label='NONE')", {number});
            emit copyOutboxes(origin.value("endpoint").toLongLong(), endpoint.value("endpoint").toLongLong());
        }
    }
    else if(endpoint.count() < 1) {
        warning() << "Cannot authorize " << number << "; invalid label " << label;
        Context::reply(event, SIP_FORBIDDEN);
        return;
    }

    auto eid = endpoint.value("endpoint").toLongLong();
    auto user = extension.value("authname").toString();
    auto authorize = getRecord("SELECT * FROM Authorize WHERE authname=?", {user});
    if(authorize.count() < 1) {
        error() << "Extension " << number << " has no authorization";
        Context::reply(event, SIP_FORBIDDEN);
        return;
    }

    auto access = authorize.value("authaccess");
    if(access == "SUSPEND") {
        warning() << "Extension " << number << " has been suspended";
        Context::reply(event, SIP_FORBIDDEN);
        return;
    }

    auto type = authorize.value("authtype").toString();
    if(type != "USER" && type != "DEVICE") {
        warning() << "Cannot authorize " << number << " as a " << type.toLower();
        Context::reply(event, SIP_FORBIDDEN);
        return;
    }

    auto protocol = event.protocol();
    if(label != "NONE" && protocol == "udp" && expires > database->expiresNat)
        expires = database->expiresNat;
    else if(protocol == "udp" && expires > database->expiresUdp)
        expires = database->expiresUdp;
    else if(expires > database->expiresTcp)
        expires = database->expiresTcp;

    auto request = event.request();
    auto roffset = request.indexOf('@');
    if(roffset < 1)
        roffset = request.indexOf(':');
    if(roffset > 0)
        request = request.mid(++roffset);

    auto display = extension.value("display").toString();
    if(display.isEmpty())
        display = authorize.value("fullname").toString();
    if(display.isEmpty())
        display = user;

    auto privs = QString("none");
    auto record = getRecord("SELECT * FROM Admin WHERE (authname='system') AND (extnbr=?);", {number});
    if(record.count() > 0)
        privs = "sysadmin";
    else {
        record = getRecord("SELECT * FROM Groups WHERE (grpnbr=0) AND (extnbr=?);", {number});
        if(record.count() > 0)
            privs = "operator";
    }

    QVariantHash reply = {
        {"realm", authorize.value("realm")},
        {"user", user},
        {"display", display},
        {"digest", authorize.value("authdigest")},
        {"secret", authorize.value("secret")},
        {"number", number},
        {"label", label},
        {"endpoint", eid},
        {"origin", request},
        {"expires", expires},
        {"privs", privs},
    };
    emit createEndpoint(event, reply);
}

void Authorize::activate(const QVariantHash& config, bool opened)
{
    Q_UNUSED(config);
    if(local.isValid() && local.isOpen()) {
        local.close();
        local = QSqlDatabase();
        QSqlDatabase::removeDatabase("auth");
    }
    db = nullptr;
    if(database->isFile() && opened)
        db = &database->db;
    else if(opened) {
        local = QSqlDatabase::addDatabase(database->dbDriver, "auth");
        db = &local;
        if(!db->isValid()) {
            error() << "Invalid auth connection";
            db = nullptr;
        }
        else {
            db->setDatabaseName(database->dbName);
            if(!database->dbHost.isEmpty())
                db->setHostName(database->dbHost);
            if(database->dbPort)
                db->setPort(database->dbPort);
            if(!database->dbUser.isEmpty())
                db->setUserName(database->dbUser);
            if(!database->dbPass.isEmpty())
                db->setPassword(database->dbPass);
            if(!db->open()) {
                failed = true;
                error() << "Failed auth connection";
                local = QSqlDatabase();
                QSqlDatabase::removeDatabase("auth");
                db = nullptr;
            }
            else
                failed = false;
        }
    }
    if(db) {
        thread()->setPriority(QThread::HighPriority);
        qDebug() << "Authorization thread activated";
    }
    else
        thread()->setPriority(QThread::NormalPriority);
}

int Authorize::runQuery(const QStringList& list)
{
    int count = 0;

    if(list.count() < 1)
        return 0;

    foreach(auto request, list) {
        if(!runQuery(request))
            break;
        ++count;
    }
    qDebug() << "Performed" << count << "of" << list.count() << "auth queries";
    return count;
}

bool Authorize::resume()
{
    if(failed || &local != db)
        return false;

    local.close();
    return checkConnection();
}

bool Authorize::checkConnection()
{
    if(db == &local) {
        if(local.isOpen() && local.isValid())
            return true;

        qDebug() << "Authorize(RE-CONNECT)";
        local.close();
        if(!local.open()) {
            failed = true;
            error() << "Authorized connection failed";
            return false;
        }
        else
            failed = false;

    }
    if(!db)
        return false;

    return true;
}

bool Authorize::runQuery(const QString &request, const QVariantList &parms)
{
    if(db == &local) {
        if(!checkConnection())
            return false;

        unsigned retries = 0;

retry:
        QSqlQuery query(local);
        query.prepare(request);

        int count = -1;
            qDebug() << "Query" << request << "LIST" << parms;
        while(++count < parms.count())
            query.bindValue(count, parms.at(count));

        if(query.exec() != true) {
            if(resume() && ++retries < 3)
                goto retry;
            warning() << "Query failed; " << query.lastError().text() << " for " << query.lastQuery();
            return false;
        }
        return true;
    }
    else
        return database->runQuery(request, parms);
}

QSqlQuery Authorize::getRecords(const QString& request, const QVariantList& parms)
{
    if(db == &local) {
        if(!checkConnection())
            return QSqlQuery();

        unsigned retries = 0;

retry:
        QSqlQuery query(local);
        query.prepare(request);
        int count = -1;
        qDebug() << "Query " << request << " LIST " << parms;
        while(++count < parms.count())
            query.bindValue(count, parms.at(count));

        if(!query.exec()) {
            if(resume() && ++retries < 3)
                goto retry;
            warning() << "Query failed; " << query.lastError().text() << " for " << query.lastQuery();
            return QSqlQuery();
        }

        return query;
    }
    else
        return database->getRecords(request, parms);
}

QSqlRecord Authorize::getRecord(const QString& request, const QVariantList &parms)
{

    if(db == &local) {
        if(!checkConnection())
            return QSqlRecord();

        unsigned retries = 0;

retry:
        QSqlQuery query(local);
        query.prepare(request);
        int count = -1;
        qDebug() << "Request " << request << " LIST " << parms;
        while(++count < parms.count())
            query.bindValue(count, parms.at(count));

        if(!query.exec()) {
            if(resume() && ++retries < 3)
                goto retry;
            warning() << "Query failed; " << query.lastError().text() << " for " << query.lastQuery();
            return QSqlRecord();
        }

        if(!query.next())
            return QSqlRecord();

        return query.record();
    }
    else
        return database->getRecord(request, parms);
}
