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

    ~DatabaseEvent() final;

    Request *reply() const {
        return request;
    }

private:
    Request *request;
};

DatabaseEvent::~DatabaseEvent() = default;

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
    connect(manager, &Manager::sendDevlist, this, &Database::sendDeviceList);
    connect(manager, &Manager::sendPending, this, &Database::sendPending);
    connect(manager, &Manager::changePending, this, &Database::changePending);
    connect(this, &Database::sendMessage, manager, &Manager::sendMessage);
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

QVariant Database::insert(const QString& request, const QVariantList &parms)
{
    if(!reopen())
        return QVariant();

    QSqlQuery query(db);
    query.prepare(request);
    int count = -1;
    qDebug() << "Request " << request << " LIST " << parms;
    while(++count < parms.count())
        query.bindValue(count, parms.at(count));

    if(!query.exec()) {
        warning() << "Query failed; " << query.lastError().text() << " for " << query.lastQuery();
        return QVariant();
    }

    return query.lastInsertId();
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

    if(!query.exec()) {
        warning() << "Query failed; " << query.lastError().text() << " for " << query.lastQuery();
        return QSqlRecord();
    }

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
    bool init = Util::dbIsFile(driver);         // TODO: remove on IPL turnover

    failed = false;

    if(init && QDir::current().exists(name))    // TODO: crit on IPL turnover
        init = false;

    if(!reopen())
        return false;

    info() << "Loaded database driver " << driver;

    if(init) {                                  // init is kept for now, later ipl...
        runQuery(Util::createQuery(driver));
        runQuery("INSERT INTO Config(realm) VALUES(?);", {
                     realm});
        runQuery("INSERT INTO Switches(uuid, version) VALUES (?,?);", {
                     uuid, PROJECT_VERSION});
        runQuery("INSERT INTO Authorize(authname, authtype, authaccess) VALUES(?,?,?);", {
                     "system", "SYSTEM", "LOCAL"});
        runQuery("INSERT INTO Authorize(authname, authtype, authaccess) VALUES(?,?,?);", {
                     "nobody", "SYSTEM", "LOCAL"});
        runQuery("INSERT INTO Authorize(authname, authtype, authaccess) VALUES(?,?,?);", {
                     "anonymous", "SYSTEM", "DISABLED"});
        runQuery("INSERT INTO Authorize(authname, authtype, authaccess) VALUES(?,?,?);", {
                     "operators", "SYSTEM", "PILOT"});
        runQuery("INSERT INTO Extensions(extnbr, authname, display) VALUES (?,?,?);", {
                     0, "operators", "Operators"});
        runQuery(Util::preloadConfig(driver));
    }
    else if(Util::dbIsFile(driver)) {       // sqlite special case...
        runQuery("UPDATE Config SET realm=? WHERE id=1;", {realm});
        runQuery("VACUUM");
    }

    QSqlQuery query(db);
    query.prepare("SELECT * FROM Config;");
    if(query.exec() && query.next())
        config = query.record();

    if(config.count() < 1)
        crit(80) << "Uninitialized database for " << driver << "; use ipl loader";

    if(config.value("dialplan").toString() == "STD3") {
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
    auto id = static_cast<int>(evt->type());
    if(id < QEvent::User + 1)
        return QObject::event(evt);

    auto reply = (static_cast<DatabaseEvent *>(evt))->reply();

    // reply->value("key"), etc, to pass parms to query here!!!!

    bool opened = reopen();

    if(!opened && reply) {
        reply->notifyFailed();
        return true;
    }

    switch(id) {
    case COUNT_EXTENSIONS:
        return true;
    }
    return true;
}

void Database::copyOutbox(qlonglong source, qlonglong target)
{
    unsigned count = 0;
    auto query = getRecords("SELECT * FROM Outboxes WHERE endpoint=?", {source});
    while(query.isActive() && query.next()) {
        auto record = query.record();
        runQuery("INSERT INTO Outboxes(mid,endpoint,msgstatus) "
                 "VALUES(?,?,0);", {record.value("mid"), target});
        ++count;
    }
    qDebug() << "Copied" << count << "outboxes from" << source << "to" << target;
}

void Database::sendDeviceList(const Event& event)
{
    qDebug() << "Seeking device list" << endl;

    auto query = getRecords("SELECT * FROM Endpoints WHERE extnbr=?;", {event.number()});

    QJsonArray list;
    while(query.isActive() && query.next()) {
        qDebug() << "QUERY" << query.record();
        auto record = query.record();
        auto endpoint = record.value("endpoint").toString();
        auto extension = record.value("extnbr").toString();
        auto label = record.value("label").toString();
        auto agent = record.value("agent").toString();
        auto resgistrated = record.value("created").toString();
        auto lastOnline = record.value("lastaccess").toString();

        QJsonObject profile {
            {"e", endpoint},
            {"n", extension},
            {"u", label},
            {"a", agent},
            {"r", resgistrated},
            {"o", lastOnline},
        };
        list << profile;
    }
    QJsonDocument jdoc(list);
    auto json = jdoc.toJson(QJsonDocument::Compact);
    Context::answerWithJson(event, json);
}

void Database::messageResponse(const QByteArray& mid, const QByteArray& ep, int status)
{
    // this wont work for postgres oid's...
    qlonglong message = mid.toLongLong();
    qlonglong endpoint = ep.toLongLong();

    runQuery("UPDATE Outboxes SET msgstatus=? WHERE mid=? AND endpoint=?;",
        {status, message, endpoint});

    qDebug() << "MESSAGE RESPONSE" << message << endpoint << status;
}

void Database::localMessage(const Event& ev)
{
    Q_ASSERT(ev.message() != nullptr);
    Q_ASSERT(ev.message()->to != nullptr);
    Q_ASSERT(ev.message()->to->url != nullptr);
    Q_ASSERT(ev.message()->to->url->username != nullptr);

    auto number = ev.number();
    auto msg = ev.message();
    bool isLabeled = (ev.number() > 0 && ev.label() != "NONE");
    qlonglong self = -1, none = -1;

    UString to = msg->to->url->username;
    QList<qlonglong> sendList;  // local endpoints to send to
    QList<int> targets;         // target extension #'s

    // if from local extenion, validate it has endpoints, find outbox sync
    if(number > 0) {
        auto count = 0;
        auto query = getRecords("SELECT endpoint, label FROM Endpoints WHERE extnbr=?;", {number});
        while(query.isActive() && query.next()) {
            auto record = query.record();
            auto label = record.value("label").toString().toUtf8();
            auto endpoint = record.value("endpoint").toLongLong();
            ++count;
            // do not sync outbox to self...
            if(isLabeled && label == ev.label())
                    self = endpoint;
            else if(!isLabeled)
                continue;
            else if(label == "NONE")
                none = endpoint;
            sendList << endpoint;
        }
        // if no endpoints for sender, must be invalid
        if(!count) {
            Context::reply(ev, SIP_FORBIDDEN);
            return;
        }
    }

    if(to.isNumber()) {
        // see if group...
        auto count = 0;
        auto query = getRecords("SELECT extnbr FROM Groups WHERE grpnbr=?;", {to.toInt()});
        while(query.isActive() && query.next()) {
            auto member = query.record().value("extnbr").toInt();
            // dont send to self in group...
            if(member != number)
                targets << member;
            ++count;
        }
        if(!count)
            targets << to.toInt();
    }
    else {
        // gather extensions by auth record for named public access
        QString name = QString::fromUtf8(to);
        auto query = getRecords("SELECT extnbr FROM Extensions WHERE authname=?;", {name});
        while(query.isActive() && query.next()) {
            targets << query.record().value("extnbr").toInt();
        }
    }

    // if no targets, we definately go no further...but if qt client, we must
    // continue on to do outbox sync.
    if(targets.count() < 1 && !isLabeled) {
        Context::reply(ev, SIP_NOT_FOUND);
        return;
    }

    // now convert targets to an endpoint send list...
    foreach(auto target, targets) {
        auto query = getRecords("SELECT endpoint FROM Endpoints WHERE extnbr=?;", {target});
        while(query.isActive() && query.next())
            sendList << query.record().value("endpoint").toLongLong();
    }

    qDebug() << "*** ENDPOINTS TO PROCESS" << sendList;

    if(sendList.count() < 1) {
        if(!isLabeled)
            Context::reply(ev, SIP_NOT_FOUND);
        return;
    }

    // create datatypes for msg table insertion...
    auto msgFrom = QString::fromUtf8(UString(msg->from->url->username).unquote());
    auto msgTo = QString::fromUtf8(to);
    auto msgSubject = QString::fromUtf8(ev.subject());
    auto msgDisplay = QString::fromUtf8(ev.display());
    auto msgType = "text";
    auto msgPosted = ev.timestamp();
    auto msgExpires = ev.timestamp();
    auto msgSequence = ev.sequence();

    if(ev.expires() > 0)
        msgExpires = msgPosted.addSecs(ev.expires());
    else
        msgExpires = msgPosted.addDays(30);

    QString msgText = "";
    if(ev.contentType() == "text/plain") {
        msgText = QString::fromUtf8(ev.body());
    }

    if(number < 1)
        msgFrom = QString::fromUtf8(ev.from().toString());

    auto mid = insert("INSERT INTO Messages(msgseq, msgfrom, msgto, subject, display, posted, msgtype, msgtext, expires) "
                            "VALUES(?,?,?,?,?,?,?,?,?);", {
                                msgSequence,
                                msgFrom,
                                msgTo,
                                msgSubject,
                                msgDisplay,
                                msgPosted,
                                msgType,
                                msgText,
                                msgExpires
                            });

    if(!mid.isValid()) {
        qDebug() << "MESSAGE INSERT BAD";
        if(!isLabeled)
            Context::reply(ev, SIP_INTERNAL_SERVER_ERROR);
        return;
    }

    // later reply for other devices...
    qDebug() << "MESSAGE OK";
    if(!isLabeled)
        Context::reply(ev, SIP_OK);

    if(sendList.count() == 0)
        return;

    QVariantHash data;          // to be sent to manager for each send/sync
    data["f"] = msgFrom.toUtf8();
    data["t"] = msgTo.toInt();
    data["d"] = ev.display();
    data["c"] = ev.contentType();
    data["b"] = ev.body();
    data["s"] = ev.subject();
    data["p"] = ev.timestamp();
    data["u"] = ev.sequence();
    data["e"] = ev.expires();
    data["r"] = mid.toString().toUtf8();

    // by tracking our messages sent we can also make sure to sync new
    // extensions or even recover old messages from the server.
    foreach(auto endpoint, sendList) {
        auto msgstatus = 0;
        if(endpoint == self || endpoint == none)
            msgstatus = SIP_OK;
        auto outbox = insert("INSERT INTO Outboxes(mid, endpoint, msgstatus) "
                             "VALUES(?,?,?);", {mid, endpoint, msgstatus});
        if(!outbox.isValid()) {
            qDebug() << "OUTBOX FAILED FOR" << endpoint;
            continue;
        }
        if(msgstatus == SIP_OK)     // dont send if we marked them ok...
            continue;
        emit sendMessage(endpoint, data);
    }
}

void Database::changePending(qlonglong endpoint)
{
    qDebug() << "Update pending for " << endpoint;
    runQuery("UPDATE Outboxes SET msgstatus=200 WHERE msgstatus=100 AND endpoint=?;", {endpoint});
}

void Database::sendPending(const Event& event, qlonglong endpoint)
{
    qDebug() << "Seeking pending for " << event.number() << event.label();
    auto query = getRecords("SELECT * FROM Outboxes JOIN Messages ON Outboxes.mid = Messages.mid WHERE endpoint=? AND msgstatus != 200;", {endpoint});

    QJsonArray list;
    while(query.isActive() && query.next()) {
        auto record = query.record();
        QJsonObject message {
            {"f", record.value("msgfrom").toString()},
            {"t", record.value("msgto").toString()},
            {"d", record.value("display").toString()},
            {"b", record.value("msgtext").toString()},
            {"c", record.value("msgtype").toString()},
            {"s", record.value("subject").toString()},
            {"p", record.value("posted").toDateTime().toString(Qt::ISODate)},
            {"u", record.value("msgseq").toInt()},
            {"e", record.value("expires").toDateTime().toString(Qt::ISODate)},
        };

        // qDebug() << "*** PENDING" << message << record.value("msgstatus").toInt();
        list.insert(0, message);    // reverse order...
    }

    runQuery("UPDATE Outboxes SET msgstatus=100 WHERE msgstatus != 200 AND endpoint=?;", {endpoint});

    if(list.count() < 1)
        Context::reply(event, SIP_OK);
    else {
        QJsonDocument jdoc(list);
        Context::answerWithJson(event, jdoc.toJson(QJsonDocument::Compact));
    }
}

void Database::sendRoster(const Event& event)
{
    qDebug() << "Seeking roster for" << event.number();

    auto query = getRecords("SELECT * FROM Extensions JOIN Authorize ON Extensions.authname = Authorize.authname ORDER BY Extensions.extnbr");

    QJsonArray list;
    while(query.isActive() && query.next()) {
        auto record = query.record();
        auto name = record.value("authname").toString();
        auto display = record.value("display").toString();
        auto dialing = record.value("extnbr").toString();
        auto email = record.value("email").toString();
        auto access = record.value("authaccess").toString();
        if(display.isEmpty())
            display = record.value("fullname").toString();
        if(display.isEmpty())
            display = record.value("authname").toString();

        UString uri = event.uriTo(dialing);
        QJsonObject profile {
            {"a", name},
            {"n", record.value("extnbr").toInt()},
            {"u", QString::fromUtf8(uri)},
            {"d", display},
            {"t", record.value("authtype").toString()},
        };

        if(!email.isEmpty())
            profile.insert("e", email);

        if(record.value("authaccess").toString() == "REMOTE")
            profile.insert("p", name + "@" + QString::fromUtf8(Server::sym(CURRENT_NETWORK)));

        // qDebug() << "*** CONTACT" << profile;
        list << profile;
    }
    qDebug() << "Extension list" << list.count();
    QJsonDocument jdoc(list);
    auto json = jdoc.toJson(QJsonDocument::Compact);
    Context::answerWithJson(event, json);
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
