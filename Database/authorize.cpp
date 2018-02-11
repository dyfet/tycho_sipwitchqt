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
        thread->setPriority(QThread::HighPriority);
        moveToThread(thread);
    }
    else
        moveToThread(database->thread());

    connect(thread(), &QThread::finished, this, &QObject::deleteLater);
    connect(database, &Database::updateAuthorize, this, &Authorize::activate);

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

    auto endpoint = getRecord("SELECT * FROM Endpoints WHERE (number=?) AND (label=?)", {number, label});
    auto extension = getRecord("SELECT * FROM Extensions WHERE number=?", {number});

    if(extension.count() < 1) {
        warning() << "Cannot authorize " << number << "; not found";
        Context::reply(event, SIP_NOT_FOUND);
        return;
    }

    // a sipwitch client specific registration feature...
    if(event.initialize() == "label") {
        if(endpoint.count() > 0) {
            warning() << "Cannot initially authorize " << number << "; label " << label << " already exists";
            Context::reply(event, SIP_CONFLICT);
            return;
        }
        runQuery("INSERT INTO Endpoints(number, label) VALUES (?,?);", {number, label});
    }
    else if(endpoint.count() < 1) {
        warning() << "Cannot authorize " << number << "; invalid label " << label;
        Context::reply(event, SIP_FORBIDDEN);
        return;
    }

    QString user = extension.value("name").toString();
    auto authorize = getRecord("SELECT * FROM Authorize WHERE name=?", {user});
    if(authorize.count() < 1) {
        error() << "Extension " << number << " has no authorization";
        Context::reply(event, SIP_FORBIDDEN);
        return;
    }

    QString type = authorize.value("type").toString();
    if(type != "USER" && type != "DEVICE") {
        warning() << "Cannot authorize " << number << " as a " << type.toLower();
        Context::reply(event, SIP_FORBIDDEN);
        return;
    }

    if(type == "DEVICE" && expires > database->expiresDevice)
        expires = database->expiresDevice;
    else if(label != "NONE" && expires > database->expiresLabeled)
        expires = database->expiresLabeled;
    else if(expires > database->expiresUser)
        expires = database->expiresUser;

    QVariantHash reply = {
        {"realm", authorize.value("realm")},
        {"user", user},
        {"digest", authorize.value("digest")},
        {"secret", authorize.value("secret")},
        {"number", number},
        {"label", label},
        {"expires", expires},
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
        local = QSqlDatabase::addDatabase(database->driver, "auth");
        db = &local;
        if(!db->isValid()) {
            error() << "Invalid auth connection";
            db = nullptr;
        }
        else {
            db->setDatabaseName(database->name);
            if(!database->host.isEmpty())
                db->setHostName(database->host);
            if(database->port)
                db->setPort(database->port);
            if(!database->user.isEmpty())
                db->setUserName(database->user);
            if(!database->pass.isEmpty())
                db->setPassword(database->pass);
            if(!db->open()) {
                error() << "Failed auth connection";
                local = QSqlDatabase();
                QSqlDatabase::removeDatabase("auth");
                db = nullptr;
            }
        }
    }
    if(db)
        qDebug() << "Authorization thread activated";
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

bool Authorize::runQuery(const QString &request, const QVariantList &parms)
{
    if(db == &local) {
        QSqlQuery query(local);
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
    else
        return database->runQuery(request, parms);
}

QSqlRecord Authorize::getRecord(const QString& request, const QVariantList &parms)
{

    if(db == &local) {
        QSqlQuery query(local);
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
    else
        return database->getRecord(request, parms);
}
