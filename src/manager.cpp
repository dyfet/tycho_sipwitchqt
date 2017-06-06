/**
 ** Copyright 2017 Tycho Softworks.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "compiler.hpp"
#include "server.hpp"
#include "control.hpp"
#include "logging.hpp"
#include "manager.hpp"
#include "main.hpp"

#include <QUuid>

QString Manager::ServerMode;
QString Manager::SystemPassword;

Manager::Manager(unsigned order) :
Stack(order)
{
    Control *control = Control::instance();
    Server *server = Server::instance();
	Database *db = Database::instance();

    connect(thread(), &QThread::finished, this, &QObject::deleteLater);
	connect(control, &Control::changeValue, this, &Manager::applyValue);
	connect(server, &Server::changeConfig, this, &Manager::applyConfig);

#ifndef QT_NO_DEBUG
    connect(db, &Database::countResults, this, &Manager::reportCounts);
#endif
}

Manager::~Manager()
{
}

void Manager::init(unsigned order)
{
    Q_ASSERT(Stack::Instance == nullptr);
    new Manager(order);
}

#ifndef QT_NO_DEBUG
void Manager::reportCounts(const QString& id, int count)
{
    qDebug() << "*** DB Count" << id << count;
}
#endif

void Manager::applyNames()
{
    QStringList names = ServerAliases + ServerNames;
    qDebug() << "Apply names" << names;
    foreach(auto context, Context::contexts()) {
        context->setOtherNames(names);
    }
}

void Manager::applyValue(const QString& id, const QVariant& value)
{
	if(id == "aliases") {
		ServerAliases = value.toStringList();
        applyNames();
	}
    else if(id == "mode") {
        ServerMode = value.toString();
        Logging::info() << "setting mode " << value.toString();
    }
}

void Manager::applyConfig(const QVariantHash& config)
{
	ServerNames = config["localnames"].toStringList();
    QString digest = config["digest"].toString().toLower();
    if(digest == "sha2" || digest == "sha256" || digest == "sha-256")
        Digest = QCryptographicHash::Sha256;
    else if(digest == "sha512" || digest == "sha-512")
        Digest = QCryptographicHash::Sha512; 
    QString realm = config["realm"].toString();
    bool genpwd = false;
    if(realm.isEmpty()) {
        genpwd = true;
        realm = Server::sym(CURRENT_NETWORK);
        if(realm.isEmpty() || realm == "local" || realm == "localhost" || realm == "localdomain")
            realm = Server::sym(CURRENT_UUID);
    }
    if(realm != ServerRealm) {
        genpwd = true;
        ServerRealm = realm;
        Logging::info() << "entering realm " << ServerRealm;
    }
    if(genpwd) { 
        SystemPassword = computeDigest("system", QUuid::createUuid().toString());
        emit changeRealm(ServerRealm, SystemPassword);   
    }
 
    applyNames();
    QList<Subnet> subnets4, subnets6;
    foreach(auto net, config["subnets"].toStringList()) {
        Subnet sub(net);
        if(sub.protocol() == QAbstractSocket::IPv4Protocol)
            subnets4 << sub;
        else
            subnets6 << sub;
    }

    if(!Context::isSeparated()) {
        foreach(auto context, Context::contexts()) {
            if(context->protocol() == QAbstractSocket::IPv4Protocol) 
                context->setOtherNets(subnets4);
            else
                context->setOtherNets(subnets6);
        }
    }
}

void Manager::createContext(const QString& name, const QHostAddress& addr, int port, unsigned mask)
{
    Q_UNUSED(name);

    qDebug().nospace() << "Creating " << name << " " <<  addr << ", port=" << port << ", mask=" << QString("0x%1").arg(mask, 8, 16, QChar('0'));
    Context::setGateway(name);

    foreach(auto schema, Context::schemas()) {
        if(schema.proto & mask) {
            new Context(addr, port, name, schema);
        }
    }
}

void Manager::createContexts(const QList<QHostAddress>& list, int port, unsigned  mask)
{
    unsigned count = 0;
    foreach(auto host, list) {
        createContext(QString("local") + QString::number(++count), host, port, mask);
    }
}

