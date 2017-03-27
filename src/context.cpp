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

#include <QHostAddress>
#include <QNetworkInterface>
#include <QHostInfo>

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

// allows task() to cleanly auto-delete eXosip events on premature returns
class auto_event
{
    Q_DISABLE_COPY(auto_event)

public:
    auto_event(eXosip_event_t *sevent) {
        event = sevent;
    }

    ~auto_event() {
        if(event) {
            eXosip_event_free(event);
            event = nullptr;
        }
    }

    operator bool() const {
        return event != nullptr;
    }

    const eXosip_event_t *operator->() const {
        return event;
    }

    const eXosip_event_t *operator*() const {
        return event;
    }

private:
    eXosip_event_t *event;
};

static volatile unsigned instanceCount = 0;
static QMutex nameLock, netLock;
static bool active = true;

bool Context::Separated = false;
QString Context::Gateway;
QList<Context *> Context::Contexts;
QList<Context::Schema> Context::Schemas = {
    {"udp", "sip:",  Context::UDP},
    {"tcp", "sip:",  Context::TCP},
    {"tls", "sips:", Context::TLS},
    {"dtls","sips:", Context::DTLS},
};

Context::Context(const QHostAddress& addr, int port, const QString& name, const Schema& choice) :
schema(choice), context(nullptr), netFamily(AF_INET), netPort(port), netTLS(0)
{
    netPort &= 0xfffe;
    
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
        netAddr = addr.toString().toUtf8();

    setObjectName(name + "/" + choice.name);
    Contexts << this;

    if(Separated)
        return;

    auto nets = QNetworkInterface::allInterfaces();
    foreach(auto net, nets) {
        foreach(auto entry, net.addressEntries()) {
            auto proto = entry.ip().protocol();
            if(addr.isNull() ||\
              (addr == QHostAddress::Any && proto == QAbstractSocket::IPv4Protocol) ||\
              (addr == QHostAddress::AnyIPv6 && proto == QAbstractSocket::IPv6Protocol) ||\
              (entry.ip() == addr && proto == addr.protocol())) {
                Subnet sub(entry.ip(), entry.prefixLength());
                localSubnets <<  sub;
            }
        }
    }
}

Context::~Context()
{
    if(context)
        eXosip_quit(context);
}

QAbstractSocket::NetworkLayerProtocol Context::protocol()
{
    if(netFamily == AF_INET)
        return QAbstractSocket::IPv4Protocol;
    else
        return QAbstractSocket::IPv6Protocol;
}

void Context::init()
{
    if(!netAddr.isEmpty() && netAddr != "0.0.0.0") {
        QHostInfo host = QHostInfo::fromName(netAddr);
        if(host.error() == QHostInfo::NoError)
            localHosts << host.hostName();
        localHosts << netAddr;
    }
    localHosts << QHostInfo::localHostName() << Util::localDomain();
}

void Context::run()
{
    ++instanceCount;

    init();

    Logging::debug() << "Running " << objectName();

    const char *ap = nullptr;
    int proto = IPPROTO_UDP;

    if(!netAddr.isEmpty()) {
        ap = netAddr.constData();
    }
    else if(netAddr == "0.0.0.0")
        ap = nullptr;

    switch(schema.proto) {
    case Context::DTLS:
        proto = IPPROTO_UDP;
        netTLS = 1;
        ++netPort;
        break;
    case Context::TLS:
        proto = IPPROTO_TCP;
        netTLS = 1;
        ++netPort;
        break;
    case Context::TCP:
        proto = IPPROTO_TCP;
        break;
    default:
        break;
    }   

    priorEvent = 0;
    
    //qDebug() << "LISTEN " << proto << ap << port << family << NetTLS;
    if(eXosip_listen_addr(context, proto, ap, netPort, netFamily, netTLS)) {
        Logging::err() << objectName() << ": failed to bind and listen";
        context = nullptr;
    }

    while(active && context) {
        int s = EVENT_TIMER / 1000l;
        int ms = EVENT_TIMER % 1000l;
        auto_event event(eXosip_event_wait(context, s, ms));
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
            Logging::debug() << "Sip Event " << event->type << "/" << eid(event->type) << ": cid=" << event->cid << ", did=" << event->did << ", proto=" << schema.name;

        if(Server::state() == Server::UP && process(*event))
            continue;
    }
    Logging::debug() << "Exiting " << objectName();
    emit finished();
    --instanceCount;
}

bool Context::process(const eXosip_event_t *ev) {
    Q_UNUSED(ev);
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

const QStringList Context::localnames()
{
    QStringList names = localHosts;
    QMutexLocker lock(&nameLock);
    names << otherNames;
    return names;
}

const QList<Subnet> Context::localnets()
{
    QList<Subnet> nets;
    if(Separated)
        return nets;

    nets << localSubnets;
    QMutexLocker lock(&netLock);
    nets << otherNets;
    return nets;
}

const char *Context::eid(eXosip_event_type ev)
{
    switch(ev) {
    case EXOSIP_REGISTRATION_SUCCESS:
        return "register";
    case EXOSIP_CALL_INVITE:
        return "invite";
    case EXOSIP_CALL_REINVITE:
        return "reinvite";
    case EXOSIP_CALL_NOANSWER:
    case EXOSIP_SUBSCRIPTION_NOANSWER:
    case EXOSIP_NOTIFICATION_NOANSWER:
        return "noanswer";
    case EXOSIP_MESSAGE_PROCEEDING:
    case EXOSIP_NOTIFICATION_PROCEEDING:
    case EXOSIP_CALL_MESSAGE_PROCEEDING:
    case EXOSIP_SUBSCRIPTION_PROCEEDING:
    case EXOSIP_CALL_PROCEEDING:
        return "proceed";
    case EXOSIP_CALL_RINGING:
        return "ring";
    case EXOSIP_MESSAGE_ANSWERED:
    case EXOSIP_CALL_ANSWERED:
    case EXOSIP_CALL_MESSAGE_ANSWERED:
    case EXOSIP_SUBSCRIPTION_ANSWERED:
    case EXOSIP_NOTIFICATION_ANSWERED:
        return "answer";
    case EXOSIP_SUBSCRIPTION_REDIRECTED:
    case EXOSIP_NOTIFICATION_REDIRECTED:
    case EXOSIP_CALL_MESSAGE_REDIRECTED:
    case EXOSIP_CALL_REDIRECTED:
    case EXOSIP_MESSAGE_REDIRECTED:
        return "redirect";
    case EXOSIP_REGISTRATION_FAILURE:
        return "noreg";
    case EXOSIP_SUBSCRIPTION_REQUESTFAILURE:
    case EXOSIP_NOTIFICATION_REQUESTFAILURE:
    case EXOSIP_CALL_REQUESTFAILURE:
    case EXOSIP_CALL_MESSAGE_REQUESTFAILURE:
    case EXOSIP_MESSAGE_REQUESTFAILURE:
        return "failed";
    case EXOSIP_SUBSCRIPTION_SERVERFAILURE:
    case EXOSIP_NOTIFICATION_SERVERFAILURE:
    case EXOSIP_CALL_SERVERFAILURE:
    case EXOSIP_CALL_MESSAGE_SERVERFAILURE:
    case EXOSIP_MESSAGE_SERVERFAILURE:
        return "server";
    case EXOSIP_SUBSCRIPTION_GLOBALFAILURE:
    case EXOSIP_NOTIFICATION_GLOBALFAILURE:
    case EXOSIP_CALL_GLOBALFAILURE:
    case EXOSIP_CALL_MESSAGE_GLOBALFAILURE:
    case EXOSIP_MESSAGE_GLOBALFAILURE:
        return "global";
    case EXOSIP_CALL_ACK:
        return "ack";
    case EXOSIP_CALL_CLOSED:
    case EXOSIP_CALL_RELEASED:
        return "bye";
    case EXOSIP_CALL_CANCELLED:
        return "cancel";
    case EXOSIP_MESSAGE_NEW:
    case EXOSIP_CALL_MESSAGE_NEW:
    case EXOSIP_IN_SUBSCRIPTION_NEW:
        return "new";
    case EXOSIP_SUBSCRIPTION_NOTIFY:
        return "notify";
    default:
        break;
    }
    return "unknown";
}





