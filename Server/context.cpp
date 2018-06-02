/*
 * Copyright (C) 2017-2018 Tycho Softworks.
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

#include "server.hpp"
#include "output.hpp"
#include "manager.hpp"

#include <QNetworkInterface>
#include <QJsonDocument>

#if defined(Q_OS_WIN)
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define EVENT_TIMER 500l    // 500ms...

#define MSG_IS_ROSTER(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method,"X-ROSTER"))

#define MSG_IS_PROFILE(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, "X-PROFILE"))

#define MSG_IS_COVERAGE(msg)  (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, "X-COVERAGE"))

#define MSG_IS_FORWARDING(msg)  (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, "X-FORWARDING"))

#define MSG_IS_MEMBERSHIP(msg)  (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, "X-MEMBERSHIP"))

#define MSG_IS_TOPIC(msg)  (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, "X-TOPIC"))

#define MSG_IS_DEVLIST(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method,"X-DEVLIST"))

#define MSG_IS_PENDING(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, "X-PENDING"))

#define MSG_IS_AUTHORIZE(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, "X-AUTHORIZE"))

#define MSG_IS_DEAUTHORIZE(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, "X-DEAUTHORIZE"))

#define MSG_IS_ACK_PENDING(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, "A-PENDING"))

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
    inline explicit ContextLocker(eXosip_t *ctx) :
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

void Context::dump(const osip_message_t *msg)
{
    if(!msg)
        return;

    char *data = nullptr;
    size_t len;
    osip_message_to_str(const_cast<osip_message_t*>(msg), &data, &len);
    if(data) {
        data[len] = 0;
        qDebug() << "MSG" << data;
        osip_free(data);
    }
}

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
    auto database = Database::instance();

    if(allow & Allow::REGISTRY)
        connect(this, &Context::REQUEST_REGISTER, stack, &Manager::refreshRegistration);

    if(netProto == IPPROTO_TCP) {
        connect(this, &Context::REQUEST_ROSTER, stack, &Manager::requestRoster);
        connect(this, &Context::REQUEST_PROFILE, stack, &Manager::requestProfile);
        connect(this, &Context::REQUEST_DEVLIST, stack, &Manager::requestDevlist);
        connect(this, &Context::REQUEST_PENDING, stack, &Manager::requestPending);
        connect(this, &Context::REQUEST_AUTHORIZE, stack, &Manager::requestAuthorize);
        connect(this, &Context::REQUEST_DEAUTHORIZE, stack, &Manager::requestDeauthorize);
        connect(this, &Context::REQUEST_MEMBERSHIP, stack, &Manager::requestMembership);
        connect(this, &Context::REQUEST_FORWARDING, stack, &Manager::requestForwarding);
        connect(this, &Context::REQUEST_COVERAGE, stack, &Manager::requestCoverage);
        connect(this, &Context::REQUEST_TOPIC, stack, &Manager::requestTopic);
        connect(this, &Context::ACK_PENDING, stack, &Manager::ackPending);
    }

    connect(this, &Context::LOCAL_MESSAGE, database, &Database::localMessage);
    connect(this, &Context::MESSAGE_RESPONSE, database, &Database::messageResponse);

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

        if(!event) {
            priorEvent = currentEvent;
            ContextLocker lock(context);    // scope lock automatic block...
            eXosip_automatic_action(context);
            continue;
        }

        // make sure even on high load we call automatic ops once a second
        if(currentEvent != priorEvent) {
            priorEvent = currentEvent;
            ContextLocker lock(context);    // scope lock automatic block...
            eXosip_automatic_action(context);
        }

        // skip extra code in event loop if we don't need it...
        if(Server::verbose())
            qDebug() << event;

        if(Server::state() == Server::UP && process(event)) {
            ContextLocker lock(context);
            eXosip_default_action(context, event.event());
            continue;
        }
    }
    debug() << "Exiting " << objectName();
    emit finished();
    --instanceCount;
}

void Context::messageResponse(const Event& event)
{
    osip_header_t *header = nullptr, *endpoint = nullptr;
    auto msg = event.sent();

    if(msg)
        osip_message_header_get_byname(msg, "x-mid", 0, &header);
    if(header)
        osip_message_header_get_byname(msg, "x-ep", 0, &endpoint);

    if(!header || !header->hvalue || !endpoint || !endpoint->hvalue) {
        qDebug() << "Unidentified message response";
        return;
    }
    if(event.status() > 0)
        emit MESSAGE_RESPONSE(UString(header->hvalue), UString(endpoint->hvalue), event.status());
}

bool Context::process(const Event& ev)
{
    switch(ev.type()) {
    case EXOSIP_MESSAGE_REQUESTFAILURE:
        if(MSG_IS_MESSAGE(ev.sent()))
            messageResponse(ev);
        break;
    case EXOSIP_MESSAGE_ANSWERED:
        if(MSG_IS_MESSAGE(ev.sent()))
            messageResponse(ev);
        break;
    case EXOSIP_MESSAGE_NEW:
        if(MSG_IS_OPTIONS(ev.message())) {
            if(ev.isLocal() && !ev.target().hasUser()) {
                return reply(ev, SIP_OK);
            }
            emit REQUEST_OPTIONS(ev);
            break;
        }

        if(MSG_IS_REGISTER(ev.message())) {
            if(!(allow & Allow::REGISTRY))
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            // if not our registration, deny
            if(ev.number() < 1) {
                qDebug() << "Non local registration attempt";
                return reply(ev, SIP_FORBIDDEN);
            }
            emit REQUEST_REGISTER(ev);
            break;
        }

        if(MSG_IS_ROSTER(ev.message())) {
            if(ev.number() < 1 || ev.label() == "NONE" || netProto != IPPROTO_TCP)
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            emit REQUEST_ROSTER(ev);
            return false;
        }

        if(MSG_IS_PROFILE(ev.message())) {
            auto to = ev.message()->to;
            if(!to || !to->url || !to->url->username)
                return reply(ev, SIP_ADDRESS_INCOMPLETE);
            if(ev.number() < 1 || !ev.toLocal())
                return reply(ev, SIP_FORBIDDEN);
            if(ev.label() == "NONE" || netProto != IPPROTO_TCP)
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            if(ev.contentType() != "profile/json" && ev.body().size() > 0)
                return reply(ev, SIP_NOT_ACCEPTABLE_HERE);
            emit REQUEST_PROFILE(ev);
            return false;
        }

        if(MSG_IS_COVERAGE(ev.message())) {
            auto to = ev.message()->to;
            if(!to || !to->url || !to->url->username)
                return reply(ev, SIP_ADDRESS_INCOMPLETE);
            if(ev.number() < 1 || !ev.toLocal())
                return reply(ev, SIP_FORBIDDEN);
            if(ev.label() == "NONE" || netProto != IPPROTO_TCP)
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            emit REQUEST_COVERAGE(ev);
            return false;
        }

        if(MSG_IS_FORWARDING(ev.message())) {
            auto to = ev.message()->to;
            if(!to || !to->url || !to->url->username)
                return reply(ev, SIP_ADDRESS_INCOMPLETE);
            if(ev.number() < 1 || !ev.toLocal())
                return reply(ev, SIP_FORBIDDEN);
            if(ev.label() == "NONE" || netProto != IPPROTO_TCP)
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            emit REQUEST_FORWARDING(ev);
            return false;
        }

        if(MSG_IS_MEMBERSHIP(ev.message())) {
            auto to = ev.message()->to;
            if(!to || !to->url || !to->url->username)
                return reply(ev, SIP_ADDRESS_INCOMPLETE);
            if(ev.number() < 1 || !ev.toLocal())
                return reply(ev, SIP_FORBIDDEN);
            if(ev.label() == "NONE" || netProto != IPPROTO_TCP)
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            if(ev.body().size() > 0)
                return reply(ev, SIP_NOT_ACCEPTABLE_HERE);
            emit REQUEST_MEMBERSHIP(ev);
            return false;
        }

        if(MSG_IS_TOPIC(ev.message())) {
            auto to = ev.message()->to;
            if(!to || !to->url || !to->url->username)
                return reply(ev, SIP_ADDRESS_INCOMPLETE);
            if(ev.number() < 1 || !ev.toLocal())
                return reply(ev, SIP_FORBIDDEN);
            if(ev.label() == "NONE" || netProto != IPPROTO_TCP)
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            emit REQUEST_TOPIC(ev);
            return false;
        }

        if(MSG_IS_DEVLIST(ev.message())) {
            if(ev.number() < 1 || ev.label() == "NONE" || netProto != IPPROTO_TCP)
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            emit REQUEST_DEVLIST(ev);
            return false;
        }

        if(MSG_IS_DEAUTHORIZE(ev.message())) {
            auto to = ev.message()->to;
            if(ev.number() < 1 || ev.label() == "NONE" || netProto != IPPROTO_TCP)
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            if(!to || !to->url || !to->url->username)
                return reply(ev, SIP_ADDRESS_INCOMPLETE);
            if(!ev.toLocal())
                return reply(ev, SIP_FORBIDDEN);
            emit REQUEST_DEAUTHORIZE(ev);
            return false;
        }

        if(MSG_IS_AUTHORIZE(ev.message())) {
            auto to = ev.message()->to;
            if(ev.number() < 1 || ev.label() == "NONE" || netProto != IPPROTO_TCP)
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            if(!to || !to->url || !to->url->username)
                return reply(ev, SIP_ADDRESS_INCOMPLETE);
            if(!ev.toLocal())
                return reply(ev, SIP_FORBIDDEN);
            if(ev.contentType() != "authorize/json" || ev.body().size() < 1)
                return reply(ev, SIP_NOT_ACCEPTABLE_HERE);
            emit REQUEST_AUTHORIZE(ev);
            return false;
        }

        if(MSG_IS_PENDING(ev.message())) {
            if(ev.number() < 1 || ev.label() == "NONE" || netProto != IPPROTO_TCP)
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            emit REQUEST_PENDING(ev);
            return false;
        }

        if(MSG_IS_ACK_PENDING(ev.message())) {
            if(ev.number() < 1 || ev.label() == "NONE")
                return reply(ev, SIP_METHOD_NOT_ALLOWED);
            emit ACK_PENDING(ev);
            return false;
        }

        if(MSG_IS_MESSAGE(ev.message())) {
            auto to = ev.message()->to;
            if(!to || !to->url || !to->url->username)
                return reply(ev, SIP_ADDRESS_INCOMPLETE);
            qDebug() << "*** MESSAGE " << ev.number() << ev.contentType();
            // if relaying messages between remotes, no!
            if(ev.number() < 1 && !ev.toLocal())
                return reply(ev, SIP_FORBIDDEN);

            if(ev.toLocal() && ev.number() < 0) {
                emit LOCAL_MESSAGE(ev);
            }
            else {
                auto from = ev.message()->from;
                if(!from || !from->url || !from->url->username || !from->url->host)
                    return reply(ev, SIP_ADDRESS_INCOMPLETE);
                // local to remote or remote to local have sms text limit
                if(ev.contentType() != "text/plain" || ev.body().length() > 160)
                    return reply(ev, SIP_FORBIDDEN);

                if(ev.toLocal()) {
                    // Early feedback to qt client since probably already is correct
                    // otherwise ui may really slow down sending with db queries...
                    if(ev.number() > 0 && ev.label() != "NONE")
                        Context::answerWithTimestamp(ev, SIP_OK);
                    emit LOCAL_MESSAGE(ev);
                }
                else {
                    emit OUTBOUND_MESSAGE(ev);
                }
            }
        }

        return reply(ev, SIP_NOT_ACCEPTABLE_HERE);
    default:
        break;
    }

    return true;
}

const UString Context::uriFrom(const UString& id) const
{
    if(id.indexOf('@') > 0)
        return id;
    if(id.length() == 0)
        return schema.uri + uriAddress;
    return schema.uri + id + "@" + uriAddress;
}

const UString Context::uriTo(const UString& id, const QList<QPair<UString, UString>>& args) const
{
    UString to = id;
    UString sep = "?";
    if(id.indexOf('@') < 0) {
        if(id.length() == 0)
            to = schema.uri + uriAddress;
        else
            to = schema.uri + id + "@" + uriAddress;
    }
    to = "<" + to;
    foreach(auto arg, args) {
        to += sep + arg.first + "=" + arg.second.escape();
        sep = "&";
    }
    return to + ">";
}

bool Context::message(const UString& from, const UString& to, const UString& route, QList<QPair<UString,UString>>& headers, const UString& type, const QByteArray& body)
{
    osip_message_t *msg = nullptr;
    ContextLocker lock(context);
    eXosip_message_build_request (context, &msg, "MESSAGE", to, from, route);

    if(!msg)
        return false;

    foreach(auto header, headers) {
        osip_message_set_header(msg, header.first, header.second);
    }

    osip_message_set_body(msg, body.constData(), static_cast<size_t>(body.length()));
    osip_message_set_content_type(msg, type);

    //dump(msg);
    if(!msg)
        return false;
    eXosip_message_send_request(context, msg);
    return true;
}

void Context::challenge(const Event &event, Registry* registry, bool reuse)
{
    char buf[8];
    osip_message_t *msg = nullptr;
    auto ctx = event.context();
    auto context = ctx->context;
    auto tid = event.tid();
    QByteArray random;

    if(reuse)
        random = registry->nounce();
    else {
        eXosip_generate_random(buf, sizeof(buf));
        random = QByteArray(buf, sizeof(buf));
    }

    UString nonce = random.toHex().toLower();
    UString realm = registry->realm();
    UString digest = registry->digest();
    UString user = registry->user();
    UString challenge = "Digest realm=" + realm.quote() + ", nonce=" + nonce.quote() + ", algorithm=" + digest.quote();
    UString expires = UString::number(registry->expires());

    if(!reuse)
        registry->setNounce(random);

    ContextLocker lock(context);
    eXosip_message_build_answer(context, tid, SIP_UNAUTHORIZED, &msg);
    osip_message_set_header(msg, WWW_AUTHENTICATE, challenge);
    osip_message_set_header(msg, "Expires", expires);

    // part of sipwitchqt client first trust/initial contact setup
    if(event.initialize() == "label" || event.initialize() == "user")
        osip_message_set_header(msg, "X-Authorize", user);

    eXosip_message_send_answer(context, tid, SIP_UNAUTHORIZED, msg);
}

bool Context::answerWithJson(const Event& event, const QByteArray& json)
{
    osip_message_t *msg = nullptr;
    auto context = event.context()->context;
    auto tid = event.tid();

    ContextLocker lock(context);

    eXosip_message_build_answer(context, tid, SIP_OK, &msg);
    if(!msg)
        return false;

    if(!json.isEmpty()) {
        osip_message_set_body(msg, json.constData(), static_cast<size_t>(json.length()));
        osip_message_set_content_type(msg, "application/json");
    }

    eXosip_message_send_answer(context, tid, SIP_OK, msg);
    return true;
}

bool Context::answerWithTimestamp(const Event& event, int result)
{                   
    osip_message_t *msg = nullptr;
    auto context = event.context()->context;
    auto tid = event.tid();
                
    ContextLocker lock(context);
    eXosip_message_build_answer(context, tid, SIP_OK, &msg);
    if(!msg)
        return false;
        
    UString timestamp = event.timestamp().toString(Qt::ISODate).toUtf8();
    osip_message_set_header(msg, "X-TS", timestamp);
    osip_message_set_header(msg, "X-MS", UString::number(event.sequence()));
    eXosip_message_send_answer(context, tid, result, msg);
    return true;
}

bool Context::authorize(const Event& event, const Registry* registry, const UString& xdp)
{
    osip_message_t *msg = nullptr;
    auto context = event.context()->context;
    auto tid = event.tid();
    UString expires = UString::number(registry->expires());

    ContextLocker lock(context);

    eXosip_message_build_answer(context, tid, SIP_OK, &msg);
    if(!msg)
        return false;

    osip_message_set_header(msg, "Expires", expires);
    if(!xdp.isEmpty()) {
        osip_message_set_body(msg, xdp.constData(), static_cast<size_t>(xdp.length()));
        osip_message_set_content_type(msg, "application/xdp");
    }

    eXosip_message_send_answer(context, tid, SIP_OK, msg);
    return true;
}

bool Context::reply(const Event& event, int code)
{
    osip_message_t *msg = nullptr;
    auto context = event.context()->context;
    auto tid = event.tid();
    auto did = event.did();
    auto cid = event.cid();

    Q_UNUSED(did);
    Q_UNUSED(cid);

    ContextLocker lock(context);
    switch(event.type()) {
    case EXOSIP_MESSAGE_NEW:
        eXosip_options_send_answer(context, tid, code, msg);
        return true;
    default:
        break;
    }
    warning() << "reply for unknown event";
    return false;
}

void Context::start(QThread::Priority priority)
{
    foreach(auto context, Contexts) {
        auto thread = new QThread;
        thread->setObjectName(context->objectName());
        context->moveToThread(thread);

        connect(thread, &QThread::started, context, &Context::run);
        connect(context, &Context::finished, thread, &QThread::quit);
        connect(context, &Context::finished, context, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);

        QThread::msleep(20);
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






