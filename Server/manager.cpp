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
#include "server.hpp"
#include "output.hpp"
#include "manager.hpp"
#include "main.hpp"

#include <QUuid>

Manager *Manager::Instance = nullptr;
UString Manager::ServerMode;
UString Manager::ServerHostname;
UString Manager::ServerRealm;
UString Manager::ServerBanner = "Welcome to SipWitchQt " PROJECT_VERSION;
QStringList Manager::ServerAliases;
QStringList Manager::ServerNames;
unsigned Manager::Contexts = 0;

Manager::Manager(unsigned order)
{
    qRegisterMetaType<Event>("Event");
    qRegisterMetaType<UString>("UString");

    moveToThread(Server::createThread("stack", order));
#ifndef Q_OS_WIN
    osip_trace_initialize_syslog(TRACE_LEVEL0, const_cast<char *>("sipwitchqt"));
#endif

    Server *server = Server::instance();
    Database *db = Database::instance();

    connect(thread(), &QThread::finished, this, &QObject::deleteLater);
    connect(server, &Server::changeConfig, this, &Manager::applyConfig);

#ifndef QT_NO_DEBUG
    connect(db, &Database::countResults, this, &Manager::reportCounts);
#endif
}

Manager::~Manager()
{
    Instance = nullptr;
}

void Manager::init(unsigned order)
{
    Q_ASSERT(Instance == nullptr);
    Instance = new Manager(order);
}

#ifndef QT_NO_DEBUG
void Manager::reportCounts(const QString& id, int count)
{
    qDebug() << "DB Count" << id << count;
}
#endif

void Manager::applyNames()
{
    QStringList names =  ServerAliases + ServerNames;
    qDebug() << "Apply names" << names;
    foreach(auto context, Context::contexts()) {
        context->applyHostnames(names, ServerHostname);
    }
}

void Manager::applyConfig(const QVariantHash& config)
{
    ServerNames = config["localnames"].toStringList();
    if(!config["banner"].toString().isEmpty())
        ServerBanner = config["banner"].toString();
    QString hostname = config["host"].toString();
    QString realm = config["realm"].toString();

    if(realm.isEmpty()) {
        realm = Server::sym(CURRENT_NETWORK);
        if(realm.isEmpty() || realm == "local" || realm == "localhost" || realm == "localdomain")
            realm = Server::uuid();
    }
    if(hostname != ServerHostname) {
        ServerHostname = hostname;
        info() << "starting as host " << ServerHostname;
    }
    if(realm != ServerRealm) {
        ServerRealm = realm;
        info() << "entering realm " << ServerRealm;
        emit changeRealm(ServerRealm);
    }
    applyNames();
}

const QByteArray Manager::computeDigest(const UString& id, const UString& secret, QCryptographicHash::Algorithm digest)
{
    if(secret.isEmpty() || id.isEmpty())
        return QByteArray();

    return QCryptographicHash::hash(id + ":" + realm() + ":" + secret, digest);
}

void Manager::create(const QHostAddress& addr, quint16 port, unsigned mask)
{
    unsigned index = ++Contexts;

    debug() << "Creating sip" << index << ": bind=" <<  addr.toString() << ", port=" << port << ", mask=" << QString("0x%1").arg(mask, 8, 16, QChar('0'));

    foreach(auto schema, Context::schemas()) {
        if(schema.proto & mask) {
            new Context(addr, port, schema, mask, index);
        }
    }
}

void Manager::create(const QList<QHostAddress>& list, quint16 port, unsigned  mask)
{
    foreach(auto host, list) {
        create(host, port, mask);
    }
}

void Manager::refreshRegistration(const Event &ev)
{
    if(ev.number() < 1) {
        Context::reply(ev, SIP_FORBIDDEN);
        return;
    }
    auto *reg = Registry::find(ev);
    if(reg) {
        if(!ev.authorization())
            Context::challenge(ev, reg);
        else {
            auto result = reg->authorize(ev);
            if(result == SIP_OK)
                Context::authorize(ev, reg);
            else
                Context::reply(ev, result);
            // releasing registration expires object
            if(reg->hasExpired())
                delete reg;
        }
    }
    else
        emit findEndpoint(ev);
}

void Manager::createRegistration(const Event& event, const QVariantHash& endpoint)
{
    if(endpoint.isEmpty()) {
        Context::reply(event, SIP_NOT_FOUND);
    }
    else {
        Registry *reg = new Registry(endpoint);
        if(!reg)
            Context::reply(event, SIP_INTERNAL_SERVER_ERROR);
        else
            Context::challenge(event, reg);
    }
}

