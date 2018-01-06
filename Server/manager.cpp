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

static QHash<QCryptographicHash::Algorithm,QByteArray> digests = {
    {QCryptographicHash::Md5,       "MD5"},
    {QCryptographicHash::Sha256,    "SHA-256"},
    {QCryptographicHash::Sha512,    "SHA-512"},
};

Manager *Manager::Instance = nullptr;
UString Manager::ServerMode;
UString Manager::ServerHostname;
UString Manager::ServerRealm;
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

void Manager::sipRegister(const Event &ev)
{
    if(ev.authorization()) {
        // TODO: if authorized, should match record in registry, else new
        // with db request or fail auth if secret doesn't match up
        Registry::process(ev);
    }
    else {
        // TODO: should search registry first, if not found, generate db auth
        // request, else uses registry auth info...
        Registry::authorize(ev);
    }


}


