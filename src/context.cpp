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

#include "server.hpp"
#include "logging.hpp"
#include "control.hpp"
#include "stack.hpp"

#include <QNetworkInterface>

#ifdef Q_OS_UNIX
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#ifdef Q_OS_WIN
#include <WinSock2.h>
#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

#define EVENT_TIMER 500l    // 500ms...

static volatile unsigned instanceCount = 0;
static bool active = true;

QList<Context *> Context::Contexts;
QList<Context::Schema> Context::Schemas = {
    {"udp", "sip:",  Context::UDP, 5060, IPPROTO_UDP},
    {"tcp", "sip:",  Context::TCP, 5060, IPPROTO_TCP},
    {"tls", "sips:", Context::TLS, 5061, IPPROTO_TCP},
    {"dtls","sips:", Context::DTLS, 5061, IPPROTO_UDP},
};

Context::Context(const QHostAddress& addr, int port, const Schema& choice, unsigned mask, unsigned index):
schema(choice), context(nullptr), netFamily(AF_INET), netPort(port)
{
    allow = mask & 0xffffff00;
    netPort &= 0xfffe;
    netPort |= (schema.inPort & 0x01);
    netProto = schema.inProto;
    netTLS = (schema.inPort & 0x01);

    context = eXosip_malloc();
    eXosip_init(context);
    eXosip_set_user_agent(context, Stack::agent());

    auto proto = addr.protocol();
    int ipv6 = 0;
    if(proto == QAbstractSocket::IPv6Protocol) {
        netFamily = AF_INET6;
        ipv6 = 1;
    }
    eXosip_set_option(context, EXOSIP_OPT_ENABLE_IPV6, &ipv6);

    if(!addr.isNull())
        netAddress = addr.toString().toUtf8();

    setObjectName(QString("sip") + QString::number(index) + "/" + choice.name);
    Contexts << this;

    if(addr != QHostAddress::Any && addr != QHostAddress::AnyIPv4 && addr != QHostAddress::AnyIPv6) {
        auto nets = QNetworkInterface::allInterfaces();
        foreach(auto net, nets) {
            foreach(auto entry, net.addressEntries()) {
                auto proto = entry.ip().protocol();
                if(entry.ip() == addr && proto == addr.protocol()) {
                    localSubnet = Subnet(entry.ip(), entry.prefixLength());
                    break;
                }
            }
        }

        if(ipv6)
            uriAddress = "[" + netAddress + "]";
        else
            uriAddress = netAddress;
    }
    else {
        uriAddress = QHostInfo::localHostName();
    }

    // reverse lookup so we can use interface name rather than addr...
    if(!netAddress.isEmpty() && netAddress != "0.0.0.0") {
        QHostInfo host = QHostInfo::fromName(netAddress);
        if(host.error() == QHostInfo::NoError)
            localHosts << host.hostName();
        localHosts << netAddress;
    }
    localHosts << QHostInfo::localHostName() << Util::localDomain();

    if(netPort != schema.inPort)
        uriAddress += ":" + QString::number(netPort);

    //qDebug() << "****** URI TO " << uriTo(QHostAddress("4.2.2.1"));
    //qDebug() << "**** LOCAL URI" << uri();
}

Context::~Context()
{
    if(context)
        eXosip_quit(context);
}

const QString Context::uriTo(const QHostAddress& target) const
{
    QString peer = uriPeer(target);
    if(peer.isNull())
        return QString();
    return schema.uri + peer;
}

const QString Context::uriTo(const QHostAddress& target, const QString& id) const
{
    QString peer = uriPeer(target);
    if(peer.isNull())
        return QString();
    return schema.uri + id + "@" + peer;
}

bool Context::isLocal(const QHostAddress& target) const
{
    // check our subnet lists
    foreach(auto subnet, localnets()) {
        if(subnet.contains(target)) {
            return true;
        }
    }

    // check local interfaces if multi-bound context
    if(!localSubnet) {
        auto nets = QNetworkInterface::allInterfaces();
        foreach(auto net, nets) {
            foreach(auto entry, net.addressEntries()) {
                if(entry.ip().protocol() != target.protocol())
                    continue;
                Subnet sub(entry.ip(), entry.prefixLength());
                if(sub.contains(target)) {
                    return true;
                }
            }
        }
    }
    return false;
}

const QString Context::uriPeer(const QHostAddress& target) const
{
    QString uriHost;

    // check our subnet lists
    foreach(auto subnet, localnets()) {
        if(subnet.contains(target)) {
            return uriAddress;
        }
    }

    // check local interfaces if multi-bound context
    if(!localSubnet) {
        auto nets = QNetworkInterface::allInterfaces();
        foreach(auto net, nets) {
            foreach(auto entry, net.addressEntries()) {
                if(entry.ip().protocol() != target.protocol())
                    continue;
                Subnet sub(entry.ip(), entry.prefixLength());
                if(sub.contains(target)) {
                    uriHost = entry.ip().toString().toUtf8();
                    if(target.protocol() == QAbstractSocket::IPv6Protocol)
                        uriHost = "[" + uriHost + "]";
                    if(netPort != schema.inPort)
                        uriHost = uriHost + ":" + QString::number(netPort);
                    return uriHost;
                }
            }
        }
    }

    if(!(allow & Allow::REMOTE))
        return QString();

    QMutexLocker lock(&nameLock);
    if(publicName.length() > 0)
        uriHost = publicName;
    else
        uriHost = QHostInfo::localHostName();

    if(netPort != schema.inPort)
        return uriHost + ":" + QString::number(netPort);

    return uriHost;
}

QAbstractSocket::NetworkLayerProtocol Context::protocol()
{
    if(netFamily == AF_INET)
        return QAbstractSocket::IPv4Protocol;
    else
        return QAbstractSocket::IPv6Protocol;
}

void Context::run()
{
    ++instanceCount;

    // connect events to state handlers when we run...
    auto stack = Stack::instance();
    if(allow & Allow::REGISTRY)
        connect(this, &Context::registry, stack, &Stack::registry);

    Logging::debug() << "Running " << objectName();

    const char *ap = nullptr;

    if(!netAddress.isEmpty()) {
        ap = netAddress.constData();
    }
    else if(netAddress == "0.0.0.0")
        ap = nullptr;

    priorEvent = 0;
    
    //qDebug() << "LISTEN " << proto << ap << port << family << NetTLS;
    if(eXosip_listen_addr(context, netProto, ap, netPort, netFamily, netTLS)) {
        Logging::err() << objectName() << ": failed to bind and listen";
        context = nullptr;
    }

    while(active && context) {
        int s = EVENT_TIMER / 1000l;
        int ms = EVENT_TIMER % 1000l;
        Event event(eXosip_event_wait(context, s, ms), this);
        time(&currentEvent);
        if(currentEvent != priorEvent) {
            priorEvent = currentEvent;
            eXosip_lock(context);
            eXosip_automatic_action(context);
            eXosip_unlock(context);
        }
        if(!event)
            continue;

        // skip extra code in event loop if we don't need it...
        if(Server::verbose())
            Logging::debug() << event;

        if(Server::state() == Server::UP && process(event))
            continue;
    }
    Logging::debug() << "Exiting " << objectName();
    emit finished();
    --instanceCount;
}

bool Context::process(const Event& ev) {
    switch(ev.type()) {
    case EXOSIP_MESSAGE_NEW:
        if(MSG_IS_REGISTER(ev.request())) {
            if(!allow & Allow::REGISTRY)
                return false;
            emit registry(ev);
        }
        else
            return false;
        break;
    case EXOSIP_REGISTRATION_FAILURE:
    case EXOSIP_REGISTRATION_SUCCESS:
        if(!allow & Allow::REGISTRY)
            return false;
        emit registry(ev);
        break;
    default:
        return false;
    }

    return true;
}

void Context::start(QThread::Priority priority)
{
    foreach(auto context, Contexts) {
        QThread::msleep(20);
        QThread *thread = new QThread;
        thread->setObjectName(context->objectName());
        context->moveToThread(thread);

        connect(thread, &QThread::started, context, &Context::run);
        connect(context, &Context::finished, thread, &QThread::quit);
        connect(context, &Context::finished, context, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);
        thread->start(priority);
    }
    QThread::msleep(100);
    qDebug() << "Started contexts" << instanceCount;
}

void Context::shutdown()
{
    qDebug() << "Shutdown contexts" << instanceCount;
    active = false;

    unsigned hanged = 50;   // up to 5 seconds, after we force...

    while(instanceCount && hanged--) {
        QThread::msleep(100);
    }
}

void Context::setOtherNets(QList<Subnet> subnets) 
{
    QMutexLocker lock(&netLock);
    otherNets = subnets;
}

void Context::setOtherNames(QStringList names) 
{
    QMutexLocker lock(&nameLock);
    otherNames = names;
}

void Context::setPublicName(QString name)
{
    if(!(allow & Allow::REMOTE))
        return;

    QMutexLocker lock(&nameLock);
    publicName = name;
}

const QStringList Context::localnames() const
{
    QStringList names = localHosts;
    QMutexLocker lock(&nameLock);
    names << otherNames;
    if(publicName.length() > 0)
        names << publicName;
    names << QHostInfo::localHostName();
    return names;
}

const QList<Subnet> Context::localnets() const
{
    QList<Subnet> nets;
    if(localSubnet)
    nets << localSubnet;
    QMutexLocker lock(&netLock);
    nets << otherNets;
    return nets;
}





