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
#include "output.hpp"
#include "manager.hpp"

#include <QNetworkInterface>

#if defined(Q_OS_WIN)
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define EVENT_TIMER 500l    // 500ms...

static bool active = true;

volatile unsigned Context::instanceCount = 0;
QList<Context *> Context::Contexts;
QList<Context::Schema> Context::Schemas = {
    {"udp", "sip:",  Context::UDP, 5060, IPPROTO_UDP},
    {"tcp", "sip:",  Context::TCP, 5060, IPPROTO_TCP},
    {"tls", "sips:", Context::TLS, 5061, IPPROTO_TCP},
    {"dtls","sips:", Context::DTLS, 5061, IPPROTO_UDP},
};

// internal lock class
class ContextLocker final
{
public:
    inline ContextLocker(eXosip_t *ctx) :
    context(ctx) {
        Q_ASSERT(context != nullptr);
        eXosip_lock(context);
    }

    inline ~ContextLocker() {
        eXosip_unlock(context);
    }

private:
    eXosip_t *context;
};


Context::Context(const QHostAddress& addr, quint16 port, const Schema& choice, unsigned mask, unsigned index):
schema(choice), context(nullptr), netFamily(AF_INET), netPort(port)
{
    allow = mask & 0xffffff00;
    netPort &= 0xfffe;
    netPort |= (schema.inPort & 0x01);
    netProto = schema.inProto;
    netTLS = (schema.inPort & 0x01);

    context = eXosip_malloc();
    eXosip_init(context);
    eXosip_set_user_agent(context, UString("SipWitchQt-server/") + PROJECT_VERSION);

    auto proto = addr.protocol();
    int ipv6 = 0, rport = 1, dns = 2;
#ifdef AF_INET6
    if(proto == QAbstractSocket::IPv6Protocol || addr == QHostAddress::AnyIPv6) {
        netFamily = AF_INET6;
        ipv6 = 1;
    } 
#endif
    eXosip_set_option(context, EXOSIP_OPT_ENABLE_IPV6, &ipv6);
    eXosip_set_option(context, EXOSIP_OPT_USE_RPORT, &rport);
    eXosip_set_option(context, EXOSIP_OPT_DNS_CAPABILITIES, &dns);

    if(!addr.isNull())
        netAddress = addr.toString().toUtf8();

    setObjectName(QString("sip") + QString::number(index) + "/" + choice.name);
    Contexts << this;

    if(addr != QHostAddress::Any && addr != QHostAddress::AnyIPv4 && addr != QHostAddress::AnyIPv6) {
        multiInterface = false;
        if(ipv6)
            uriAddress = netAddress.quote("[]");
        else
            uriAddress = netAddress;
    }
    else {
        multiInterface = true;
        uriAddress = QHostInfo::localHostName().toUtf8();
    }

    // reverse lookup so we can use interface name rather than addr...
    if(!netAddress.isEmpty() && netAddress != "0.0.0.0") {
        QHostInfo host = QHostInfo::fromName(netAddress);
        if(host.error() == QHostInfo::NoError)
            localHosts << host.hostName();
        localHosts << netAddress;
    }
    localHosts << QHostInfo::localHostName() << Util::localDomain();

    uriHost = uriAddress;
    if(netPort != schema.inPort)
        uriAddress += ":" + UString::number(netPort);

    //qDebug() << "****** URI TO " << uriTo(QHostAddress("4.2.2.1"));
    //qDebug() << "**** LOCAL URI" << uri();
}

Context::~Context()
{
    if(context)
        eXosip_quit(context);
}

const UString Context::uriTo(const Contact &address) const
{
    UString host = address.host();
    UString port = "";

    if(host[0] != '[' && host.contains(":"))
        host = host.quote("[]");

    if(address.port() != schema.inPort)
        port = ":" + UString::number(address.port());

    if(address.user().length() > 0)
        return schema.uri + address.user() + "@" + host + port;
    return schema.uri + host + port;
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
    auto stack = Manager::instance();
    if(allow & Allow::REGISTRY)
        connect(this, &Context::REQUEST_REGISTER, stack, &Manager::refreshRegistration);

    debug() << "Running " << objectName();

    const char *ap = nullptr;

    if(!netAddress.isEmpty()) {
        ap = netAddress.constData();
    }
    else if(netAddress == "0.0.0.0")
        ap = nullptr;

    priorEvent = 0;
    
    //qDebug() << "LISTEN " << proto << ap << port << family << NetTLS;
    if(eXosip_listen_addr(context, netProto, ap, netPort, netFamily, netTLS)) {
        error() << objectName() << ": failed to bind and listen";
        context = nullptr;
    }

    while(active && context) {
        int s = EVENT_TIMER / 1000l;
        int ms = EVENT_TIMER % 1000l;
        Event event(eXosip_event_wait(context, s, ms), this);
        time(&currentEvent);
        if(currentEvent != priorEvent) {
            priorEvent = currentEvent;
            ContextLocker lock(context);    // scope lock automatic block...
            eXosip_automatic_action(context);
        }
        if(!event)
            continue;

        // skip extra code in event loop if we don't need it...
        if(Server::verbose())
            qDebug() << event;

        if(Server::state() == Server::UP && process(event))
            continue;
    }
    debug() << "Exiting " << objectName();
    emit finished();
    --instanceCount;
}

bool Context::process(const Event& ev)
{
    switch(ev.type()) {
    case EXOSIP_MESSAGE_NEW:
        if(MSG_IS_OPTIONS(ev.message())) {
            if(ev.isLocal() && !ev.target().hasUser()) {
                return reply(ev, SIP_OK);
            }
            emit REQUEST_OPTIONS(ev);
        }
        if(MSG_IS_REGISTER(ev.message())) {
            if(!(allow & Allow::REGISTRY))
                return false;
            emit REQUEST_REGISTER(ev);
        }
        else
            return false;
        break;
    default:
        return false;
    }

    return true;
}

void Context::challenge(const Event &event, const QSqlRecord &auth)
{
    osip_message_t *msg = nullptr;
    auto ctx = event.context();
    auto context = ctx->context;
    auto tid = event.tid();

    //TODO: a real nounce generator and cache to verify
    time_t now;
    UString nounce = UString::number(static_cast<int>(time(&now) & 0xffffffff));
    UString realm = auth.value("realm").toString().toUtf8();
    UString digest = auth.value("digest").toString().toUtf8();
    UString user = auth.value("user").toString().toUtf8();
    UString challenge = "Digest realm=" + realm.quote() + ", nounce=" + nounce.quote() + ", algorithm=" + digest.quote();

    ContextLocker lock(context);
    eXosip_message_build_answer(context, tid, SIP_UNAUTHORIZED, &msg);
    osip_message_set_header(msg, WWW_AUTHENTICATE, challenge);
    osip_message_set_header(msg, "X-Authorize", user);
    eXosip_message_send_answer(context, tid, SIP_UNAUTHORIZED, msg);
}

bool Context::reply(const Event& event, int code)
{
    osip_message_t *msg = nullptr;
    auto context = event.context()->context;
    auto tid = event.tid();
    auto did = event.did();
    auto cid = event.cid();

    ContextLocker lock(context);
    switch(event.type()) {
    case EXOSIP_MESSAGE_NEW:
//        if(MSG_IS_OPTIONS(event.message())) {
            if(code == SIP_OK) {
                eXosip_options_build_answer(context, tid, code, &msg);
                if(!msg)
                    return false;
                //TODO: fill in more headers...
            }
            eXosip_options_send_answer(context, tid, code, msg);
            return true;
//        }
        break;
    default:
        break;
    }
    warning() << "reply for unknown event";
    return false;
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
    if(!instanceCount)
        crit(99) << "** No contexts available";
    debug() << "Started contexts " << instanceCount;
}

void Context::shutdown()
{
    debug() << "Shutdown contexts " << instanceCount;
    active = false;

    unsigned hanged = 50;   // up to 5 seconds, after we force...

    while(instanceCount && hanged--) {
        QThread::msleep(100);
    }
}

const UString Context::hostname() const {
    QMutexLocker lock(&nameLock);
    if(publicName.length() > 0)
        return publicName;
    if(!multiInterface)
        return uriHost;
    return QHostInfo::localHostName();
}

void Context::applyHostnames(const QStringList& names, const QString& host)
{
    QMutexLocker lock(&nameLock);
    otherNames.clear();
    if(host.length() > 0)
        otherNames << host;
    otherNames << names;
    publicName = host.toUtf8();
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






