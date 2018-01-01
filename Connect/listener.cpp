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

Listener *Listener::Instance = nullptr;

Listener::Listener(const UString& address, quint16 port) :
QObject(), active(true)
{
    serverAddress = address;
    serverPort = port;
    family = AF_INET;
    tls = 0;

    threading();
}

void Listener::threading()
{
    QThread *thread = new QThread;
    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &Listener::run);
    connect(this, &Listener::finished, thread, &QThread::quit);
    connect(this, &Listener::finished, this, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start(QThread::HighPriority);
}

void Listener::run()
{
    int ipv6 = 0, rport = 1, dns = 2;

#ifdef AF_INET6
    if(family == AF_INET && serverAddress.contains(':')) {
        family = AF_INET6;
        ++ipv6;
    }
#endif

    context = eXosip_malloc();
    eXosip_init(context);
    eXosip_set_option(context, EXOSIP_OPT_ENABLE_IPV6, &ipv6);
    eXosip_set_option(context, EXOSIP_OPT_USE_RPORT, &rport);
    eXosip_set_option(context, EXOSIP_OPT_DNS_CAPABILITIES, &dns);
    eXosip_set_user_agent(context, UString("Antisipate/") + PROJECT_VERSION);
    eXosip_listen_addr(context, IPPROTO_UDP, NULL, 0, family, tls);

    while(active) {
        int s = EVENT_TIMER / 1000l;
        int ms = EVENT_TIMER % 1000l;
        auto event = eXosip_event_wait(context, s, ms);
        if(!active)
            break;

        // timeout...
        if(!event) {
            Locker lock(context);
            eXosip_automatic_action(context);
            continue;
        }

        // event dispatch....

        eXosip_event_free(event);
    }

    emit finished();
    eXosip_quit(context);
    context = nullptr;
}

void Listener::start(const UString& address, quint16 port)
{
    Q_ASSERT(Instance == nullptr);
    Instance = new Listener(address, port);
}

void Listener::stop()
{
    if(Instance) {
        Instance->active = false;
        Instance = nullptr;
        QThread::msleep(100);
    }
}
