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

#include "connector.hpp"

#include <QJsonDocument>
#include <QJsonObject>

#if defined(Q_OS_WIN)
#include <WinSock2.h>
#else
#include <netinet/in.h>
#endif

#define EVENT_TIMER 400l    // 400ms...

#define X_ROSTER    "X-ROSTER"
#define X_PROFILE   "X-PROFILE"
#define X_DEVLIST   "X-DEVLIST"

#define MSG_IS_ROSTER(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_ROSTER))

#define MSG_IS_DEVLIST(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_DEVLIST))

static const char *eid(eXosip_event_type ev);

static bool exiting = false;

// internal lock class
class Locker final
{
public:
    explicit inline Locker(eXosip_t *ctx) :
    context(ctx) {
        Q_ASSERT(context != nullptr);
        eXosip_lock(context);
    }

    inline ~Locker() {
        eXosip_unlock(context);
    }

private:
    eXosip_t *context;
};

static void dump(osip_message_t *msg)
{
    if(!msg)
        return;

    char *data = nullptr;
    size_t len;
    osip_message_to_str(msg, &data, &len);
    if(data) {
        data[len] = 0;
        qDebug() << "MSG" << data;
        osip_free(data);
    }
}

Connector::Connector(const QVariantHash& cred, const QSslCertificate& cert) :
QObject(), active(true)
{
    serverId = cred["extension"].toString();
    serverHost = cred["host"].toString().toUtf8();
    serverPort = static_cast<quint16>(cred["port"].toUInt());
    serverLabel = cred["label"].toString().toUtf8();
    serverDisplay = cred["display"].toString().toUtf8();

    serverCreds = cred;
    serverCreds["schema"] = "sip:";
    serverSchema = "sip:";

    if(!serverPort)
        serverPort = 5060;

    family = AF_INET;
    tls = 0;

    if(!cert.isNull()) {
        serverSchema = "sips:";
        serverCreds["schema"] = "sips:";
        ++tls;
    }

    if(!serverPort) {
        serverPort = 5060;
        if(tls)
            ++serverPort;
    }

    uriFrom = UString::uri(serverSchema, serverId, serverHost, serverPort);
    uriRoute = UString::uri(serverSchema, serverHost, serverPort);
    sipFrom = UString("\"") + serverDisplay + "\" <" + uriFrom + ">";

    auto thread = new QThread;
    this->moveToThread(thread);


    connect(thread, &QThread::started, this, &Connector::run);
    connect(this, &Connector::finished, thread, &QThread::quit);
    connect(this, &Connector::finished, this, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
}

void Connector::add_authentication()
{
    UString user = serverCreds["user"].toString();
    UString secret = serverCreds["secret"].toString();
    UString realm = serverCreds["realm"].toString();
    UString algo = serverCreds["algorithm"].toString();

    Locker lock(context);
    eXosip_add_authentication_info(context, serverId, user, secret, algo, realm);
}

void Connector::requestRoster()
{
    auto to = UString::uri(serverSchema, serverHost, serverPort);
    auto from = UString::uri(serverSchema, serverId, serverHost, serverPort);
    osip_message_t *msg = nullptr;

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_ROSTER, to, from, to);
    if(!msg)
        return;
    osip_message_set_header(msg, "X-Label", serverLabel);
    eXosip_message_send_request(context, msg);
}

void Connector::requestDeviceList()
{
    auto to = UString::uri(serverSchema, serverHost, serverPort);
    auto from = UString::uri(serverSchema, serverId, serverHost, serverPort);
    osip_message_t *msg = nullptr;

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_DEVLIST, to, from, to);
    if(!msg)
        return;
    osip_message_set_header(msg, "X-Label", serverLabel);
    eXosip_message_send_request(context, msg);
}

const UString Connector::sipTo(const UString& id, const QList<QPair<UString, UString>> args) const
{
    UString sep = "?";
    UString to = "<" + id;
    foreach(auto arg, args) {
        to += sep + arg.first + "=" + arg.second.escape();
        sep = "&";
    }
    return to + ">";
}

bool Connector::sendText(const UString& to, const UString& body, const UString subject)
{
    osip_message_t *msg = nullptr;
    Locker lock(context);
    QList<QPair<UString,UString>> args = {
        {"Subject", subject},
        // TODO: any other special X-Headers we may use here...
    };
    eXosip_message_build_request(context, &msg, "MESSAGE", sipTo(to, args), sipFrom, uriRoute);
    if(!msg)
        return false;

    osip_message_set_header(msg, "X-Label", serverLabel);
    osip_message_set_body(msg, body.constData(), static_cast<size_t>(body.length()));
    osip_message_set_content_type(msg, "text/plain");
    eXosip_message_send_request(context, msg);
    return true;

}
void Connector::run()
{
    int ipv6 = 0, rport = 1, dns = 2, live = 17000;

#ifdef AF_INET6
    if(family == AF_INET && serverHost.contains(':')) {
        family = AF_INET6;
        ++ipv6;
    }
#endif

    context = eXosip_malloc();
    eXosip_init(context);
    eXosip_set_option(context, EXOSIP_OPT_ENABLE_IPV6, &ipv6);
    eXosip_set_option(context, EXOSIP_OPT_USE_RPORT, &rport);
    eXosip_set_option(context, EXOSIP_OPT_DNS_CAPABILITIES, &dns);
    eXosip_set_option(context, EXOSIP_OPT_UDP_KEEP_ALIVE, &live);
    eXosip_set_user_agent(context, UString("SipWitchQt-client/") + PROJECT_VERSION);
    eXosip_listen_addr(context, IPPROTO_TCP, nullptr, 0, family, tls);

    emit starting();

    int s = EVENT_TIMER / 1000l;
    int ms = EVENT_TIMER % 1000l;
    int error, sequence;
    time_t now, last = 0;
    osip_header_t *header;

    qDebug() << "Connector started";
    add_authentication();

    while(active) {
        auto event = eXosip_event_wait(context, s, ms);
        if(!active)
            break;

        time(&now);

        if(!event) {
            Locker lock(context);
            last = now;
            eXosip_automatic_action(context);
            continue;
        }

        qDebug().nospace() << "tcp=" << eid(event->type) << " cid=" << event->cid;

        switch(event->type) {
        case EXOSIP_MESSAGE_REQUESTFAILURE:
            error = 666;
            if(event->response)
                error = event->response->status_code;
            if(event->response && event->response->status_code != SIP_UNAUTHORIZED)
                qDebug() << "*** REQUEST FAILURE" << event->response->status_code;
            qDebug() << event->request;
            if(MSG_IS_MESSAGE(event->request))
                emit messageResult(error, QDateTime(), 0);
            qDebug() << "*** FAILED" << error;
            if(error == 666)
                emit failure(666);
            break;
        case EXOSIP_MESSAGE_ANSWERED:
            if(!event->response)
                break;
            switch(event->response->status_code) {
            case SIP_OK:
                if(MSG_IS_ROSTER(event->request))
                    processRoster(event);
                else if(MSG_IS_DEVLIST(event->request))
                    qDebug() << "*** NEED TO PROCESS...";
                else if(MSG_IS_MESSAGE(event->request)) {
                    sequence = 0;
                    header = nullptr;
                    osip_message_header_get_byname(event->response, "x-sc", 0, &header);
                    if(header && header->hvalue)
                        sequence = atoi(header->hvalue);
                    qDebug() << "*** SEQUENCE" << sequence;
                    header = nullptr;
                    osip_message_header_get_byname(event->response, "x-ts", 0, &header);
                    if(header && header->hvalue)
                        emit messageResult(SIP_OK, QDateTime::fromString(QString(header->hvalue), Qt::ISODate), sequence);
                    else
                        emit messageResult(SIP_OK, QDateTime(), 0);
                }
                break;
            default:
                qDebug() << "*** ANSWER FAILURE" << event->response->status_code;
                break;
            }
            break;
        case EXOSIP_MESSAGE_NEW:
            if(MSG_IS_REGISTER(event->request)) {
                Locker lock(context);
                eXosip_message_send_answer(context, event->tid, SIP_METHOD_NOT_ALLOWED, nullptr);
                break;
            }
            else
                dump(event->request);
            break;
        default:
            if(event->response)
                dump(event->response);
            else if(event->request)
                dump(event->request);
            break;
        }
        if(active) {
            Locker lock(context);
            eXosip_default_action(context, event);

            // even on high load we get automatic operations in...
            if(last != now) {
                eXosip_automatic_action(context);
                last = now;
            }
        }
        eXosip_event_free(event);
    }

    qDebug() << "Connector ending";
    if(!exiting)
        emit finished();
    eXosip_quit(context);
    context = nullptr;
}

void Connector::processDeviceList(eXosip_event_t *event)
{
    osip_body_t *body = nullptr;
    osip_message_get_body(event->response, 0, &body);
    if(body && body->body && body->length > 0) {
        QByteArray json(body->body, static_cast<int>(body->length));
        emit changeDeviceList(json);
    }
}

void Connector::processRoster(eXosip_event_t *event)
{
    osip_body_t *body = nullptr;
    osip_message_get_body(event->response, 0, &body);
    if(body && body->body && body->length > 0) {
        QByteArray json(body->body, static_cast<int>(body->length));
        emit changeRoster(json);
    }
}

void Connector::stop(bool flag)
{
    exiting = flag;
    active = false;
}

static const char *eid(eXosip_event_type ev)
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
