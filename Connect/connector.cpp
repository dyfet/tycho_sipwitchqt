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

#include "connector.hpp"

#include <QJsonDocument>
#include <QJsonObject>

#if defined(Q_OS_WIN)
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#define EVENT_TIMER 400l    // 400ms...

namespace {
bool exiting = false;
}

Connector::Connector(const QVariantHash& cred, const QSslCertificate& cert) :
active(true), context(nullptr)
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

void Connector::requestDeauthorize(const UString& to)
{
    osip_message_t *msg = nullptr;
    UString sipTo = "<" + UString::uri(serverSchema, to, serverHost, serverPort) + ">";

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_DEAUTHORIZE, sipTo, sipFrom, uriRoute);
    if(!msg)
        return;

    osip_message_set_header(msg, "X-Label", serverLabel);
    eXosip_message_send_request(context, msg);
}

void Connector::removeDevice(const UString& label)
{
    osip_message_t *msg = nullptr;
    Locker lock(context);

    eXosip_message_build_request(context, &msg, X_DEVKILL, sipFrom, sipFrom, uriRoute);
    if(!msg)
        return;

    osip_message_set_header(msg, "X-Label", serverLabel);
    osip_message_set_header(msg, "X-Remove", label);
    eXosip_message_send_request(context, msg);
}

void Connector::createAuthorize(const UString& to, const QByteArray& body)
{
    osip_message_t *msg = nullptr;
    UString sipTo = "<" + UString::uri(serverSchema, to, serverHost, serverPort) + ">";

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_AUTHORIZE, sipTo, sipFrom, uriRoute);
    if(!msg)
        return;

    osip_message_set_header(msg, "X-Label", serverLabel);
    if(body.size() > 0) {
        osip_message_set_body(msg, body.constData(), static_cast<size_t>(body.length()));
        osip_message_set_content_type(msg, "authorize/json");
    }
    eXosip_message_send_request(context, msg);
}

void Connector::changeTopic(const UString& to, const UString& subject, const UString& body)
{
    osip_message_t *msg = nullptr;
    UString sipTo = "<" + to + ">";

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_TOPIC, sipTo, sipFrom, uriRoute);
    if(!msg)
        return;

    osip_message_set_header(msg, "Subject", subject);
    osip_message_set_header(msg, "X-Label", serverLabel);
    if(body.size() > 0) {
        osip_message_set_body(msg, body.constData(), static_cast<size_t>(body.length()));
        osip_message_set_content_type(msg, "text/plain");
    }
    eXosip_message_send_request(context, msg);
}

void Connector::changeForwarding(const UString& to, Forwarding type, int target)
{
    UString destination = "NONE";
    osip_message_t *msg = nullptr;
    UString sipTo = "<" + to + ">";

    if(target > -1)
        destination = UString::number(target);

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_FORWARDING, sipTo, sipFrom, uriRoute);
    if(!msg)
        return;

    switch(type) {
    case Forwarding::NA:
        osip_message_set_header(msg, "Subject", "NA");
        break;
    case Forwarding::BUSY:
        osip_message_set_header(msg, "Subject", "BUSY");
        break;
    case Forwarding::AWAY:
        osip_message_set_header(msg, "Subject", "AWAY");
        break;
    }

    osip_message_set_header(msg, "X-Label", serverLabel);
    osip_message_set_header(msg, "X-Destination", destination);
    eXosip_message_send_request(context, msg);
}

void Connector::changeCoverage(const UString& to, int priority)
{
    UString coverage = "never";
    if(priority >= 0)
        coverage = UString::number(priority);

    osip_message_t *msg = nullptr;
    UString sipTo = "<" + to + ">";

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_COVERAGE, sipTo, sipFrom, uriRoute);
    if(!msg)
        return;

    osip_message_set_header(msg, "X-Label", serverLabel);
    osip_message_set_header(msg, "X-Priority", coverage);
    eXosip_message_send_request(context, msg);
}

void Connector::changeMemebership(const UString& to, const UString& subject, const UString& list, const UString& admin, const UString& notify, const UString& reason)
{
    osip_message_t *msg = nullptr;
    UString sipTo = "<" + to + ">";

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_MEMBERSHIP, sipTo, sipFrom, uriRoute);
    if(!msg)
        return;

    osip_message_set_header(msg, "Subject", subject);
    osip_message_set_header(msg, "X-Label", serverLabel);
    osip_message_set_header(msg, "X-Group-Member", list);
    osip_message_set_header(msg, "X-Group-Admin", admin);
    osip_message_set_header(msg, "X-Notify", notify);
    osip_message_set_header(msg, "X-Reason", reason.escape());
    eXosip_message_send_request(context, msg);
}

void Connector::disconnectUser(const UString& to)
{
    osip_message_t *msg = nullptr;
    UString sipTo = "<" + to + ">";

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_DROP, sipTo, sipFrom, uriRoute);
    if(!msg)
        return;

    osip_message_set_header(msg, "X-Label", serverLabel);
    eXosip_message_send_request(context, msg);
}

void Connector::changeSuspend(const UString& to, bool suspend)
{
    osip_message_t *msg = nullptr;
    UString sipTo = "<" + to + ">";

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_ADMIN, sipTo, sipFrom, uriRoute);
    if(!msg)
        return;

    UString value = "1";
    if(!suspend)
        value = "0";

    osip_message_set_header(msg, "X-Label", serverLabel);
    osip_message_set_header(msg, "X-Suspend", value);
    eXosip_message_send_request(context, msg);
}

void Connector::changeAdmin(const UString& to, bool enable)
{
    osip_message_t *msg = nullptr;
    UString sipTo = "<" + to + ">";

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_ADMIN, sipTo, sipFrom, uriRoute);
    if(!msg)
        return;

    UString value = "1";
    if(!enable)
        value = "0";

    osip_message_set_header(msg, "X-Label", serverLabel);
    osip_message_set_header(msg, "X-System", value);
    eXosip_message_send_request(context, msg);
}

void Connector::sendProfile(const UString& to, const QByteArray& body)
{
    osip_message_t *msg = nullptr;
    UString sipTo = "<" + to + ">";

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_PROFILE, sipTo, sipFrom, uriRoute);
    if(!msg)
        return;

    osip_message_set_header(msg, "X-Label", serverLabel);
    if(body.size() > 0) {
        osip_message_set_body(msg, body.constData(), static_cast<size_t>(body.length()));
        osip_message_set_content_type(msg, "profile/json");
    }
    eXosip_message_send_request(context, msg);
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

bool Connector::sendText(const UString& to, const UString& body, const UString& subject)
{
    osip_message_t *msg = nullptr;
    UString sipTo = "<" + to + ">";
    QList<QPair<UString,UString>> hdrs = {
        {"Subject", subject},
        {"X-Label", serverLabel},
    };

    Locker lock(context);
    eXosip_message_build_request(context, &msg, "MESSAGE", sipTo, sipFrom, uriRoute);
    if(!msg)
        return false;

    foreach(auto hdr, hdrs) {
        osip_message_set_header(msg, hdr.first, hdr.second);
    }

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

        qDebug().nospace() << "tcp=" << toStr(event->type) << " cid=" << event->cid;

        switch(event->type) {
        case EXOSIP_MESSAGE_REQUESTFAILURE:
            error = 666;
            if(event->response)
                error = event->response->status_code;
            if(event->response && event->response->status_code != SIP_UNAUTHORIZED)
                qDebug() << "*** REQUEST FAILURE" << event->response->status_code;
            if(MSG_IS_TOPIC(event->request))
                emit topicFailed();
            if(MSG_IS_MESSAGE(event->request))
                emit messageResult(error, QDateTime(), 0);
            if(error != SIP_UNAUTHORIZED) {
                emit statusResult(error, "");
                qDebug() << "*** FAILED" << error;
            }
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
                else if(MSG_IS_PROFILE(event->request) || MSG_IS_COVERAGE(event->request) || MSG_IS_MEMBERSHIP(event->request) || MSG_IS_FORWARDING(event->request) || MSG_IS_DEVKILL(event->request) || MSG_IS_ADMIN(event->request) || MSG_IS_DROP(event->request))
                    processProfile(event);
                else if(MSG_IS_AUTHORIZE(event->request)) {
                    emit statusResult(SIP_OK, "");
                    UString target = "0";
                    if(event->request->to && event->request->to->url && event->request->to->url->username)
                        target = event->request->to->url->username;
                    if(target.toInt() > 0) {
                        target = UString::uri(serverSchema, target, serverHost, serverPort);
                        requestProfile(target);
                    }
                }
                else if(MSG_IS_DEAUTHORIZE(event->request))
                    processDeauthorize(event);
                else if(MSG_IS_DEVLIST(event->request))
                    processDeviceList(event);
                else if(MSG_IS_PENDING(event->request))
                    processPending(event);
                else if(MSG_IS_MESSAGE(event->request)) {
                    sequence = 0;
                    header = nullptr;
                    osip_message_header_get_byname(event->response, "x-ms", 0, &header);
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
                if(MSG_IS_TOPIC(event->request))
                    emit topicFailed();
                emit statusResult(event->response->status_code, "");
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

void Connector::processPending(eXosip_event_t *event)
{
    osip_body_t *body = nullptr;
    osip_message_get_body(event->response, 0, &body);
    if(body && body->body && body->length > 0) {
        QByteArray json(body->body, static_cast<int>(body->length));
        emit syncPending(json);
    }
}

void Connector::processDeauthorize(eXosip_event_t *event)
{
    auto msg = event->request;
    if(msg && msg->to && msg->to->url && msg->to->url->username)
        emit deauthorizeUser(UString(msg->to->url->username).unquote());
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

void Connector::processProfile(eXosip_event_t *event)
{
    osip_body_t *body = nullptr;
    osip_message_get_body(event->response, 0, &body);
    if(body && body->body && body->length > 0) {
        QByteArray json(body->body, static_cast<int>(body->length));
        emit changeProfile(json);
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

void Connector::requestPending()
{
    qDebug() << "REQUEST PENDING";
    auto to = UString::uri(serverSchema, serverHost, serverPort);
    auto from = UString::uri(serverSchema, serverId, serverHost, serverPort);
    osip_message_t *msg = nullptr;

    Locker lock(context);
    eXosip_message_build_request(context, &msg, X_PENDING, to, from, to);
    if(!msg)
        return;
    osip_message_set_header(msg, "X-Label", serverLabel);
    eXosip_message_send_request(context, msg);
}

void Connector::ackPending()
{
    qDebug() << "ACK PENDING PROCESSED";
    auto to = UString::uri(serverSchema, serverHost, serverPort);
    auto from = UString::uri(serverSchema, serverId, serverHost, serverPort);
    osip_message_t *msg = nullptr;

    Locker lock(context);
    eXosip_message_build_request(context, &msg, A_PENDING, to, from, to);
    if(!msg)
        return;
    osip_message_set_header(msg, "X-Label", serverLabel);
    eXosip_message_send_request(context, msg);
}

void Connector::stop(bool flag)
{
    qDebug() << "Stop connector" << flag;
    exiting = flag;
    active = false;
}
