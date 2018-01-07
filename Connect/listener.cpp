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

#include "listener.hpp"

#include <QTimer>

#if defined(Q_OS_WIN)
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define EVENT_TIMER 500l    // 500ms...

// internal lock class
class Locker final
{
public:
    inline Locker(eXosip_t *ctx) :
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

Listener::Listener(const QVariantHash& cred, const QSslCertificate& cert) :
QObject(), active(true), connected(false)
{
    serverHost = cred["server"].toString();
    serverPort = static_cast<quint16>(cred["port"].toUInt());
    serverUser = cred["user"].toString();
    serverSchema = "sip:";

    if(!serverPort)
        serverPort = 5060;

    family = AF_INET;
    tls = 0;
    rid = -1;

    if(!cert.isNull()) {
        serverSchema = "sips:";
        ++tls;
    }

    if(!serverPort) {
        serverPort = 5060;
        if(tls)
            ++serverPort;
    }

    QTimer::singleShot(5000, this, &Listener::timeout);
    QThread *thread = new QThread;
    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &Listener::run);
    connect(this, &Listener::finished, thread, &QThread::quit);
    connect(this, &Listener::finished, this, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
}

void Listener::run()
{
    int ipv6 = 0, rport = 1, dns = 2;

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
    eXosip_set_user_agent(context, UString("SipWitchQt-client/") + PROJECT_VERSION);
    eXosip_listen_addr(context, IPPROTO_UDP, NULL, 0, family, tls);

    emit starting();

    osip_message_t *msg = nullptr;
    UString identity = serverSchema + serverUser + "@" + serverHost + ":" + UString::number(serverPort);
    UString server = serverSchema + serverHost + ":" + UString::number(serverPort);
    rid = eXosip_register_build_initial_register(context, identity, server, NULL, 1800, &msg);
    if(msg && rid > -1) {
        osip_message_set_supported(msg, "100rel");
        osip_message_set_header(msg, "Event", "Registration");
        osip_message_set_header(msg, "Allow-Events", "presence");
        eXosip_register_send_register(context, rid, msg);
    }
    else
        active = false;

    while(active) {
        int s = EVENT_TIMER / 1000l;
        int ms = EVENT_TIMER % 1000l;
        auto event = eXosip_event_wait(context, s, ms);
        if(!active)
            break;

        // timeout...
        if(!event) {
            qDebug() << "timeout";
            Locker lock(context);
            eXosip_automatic_action(context);
            continue;
        }
        else {
            connected = true;
            qDebug() << "type=" << event->type << " cid=" << event->cid;
        }

        // event dispatch....

        eXosip_event_free(event);
    }

    // de-register if we are ending the session while registered
    if(rid > -1) {
        osip_message_t *msg = nullptr;
        Locker lock(context);
        eXosip_register_build_register(context, rid, 0, &msg);
        if(msg)
            eXosip_register_send_register(context, rid, msg);
    }

    emit finished();
    eXosip_quit(context);
    context = nullptr;
}

void Listener::timeout()
{
    if(connected)
        return;

    emit failure(666);
    active = false;
}

void Listener::stop()
{
    active = false;
    QThread::msleep(100);
}
