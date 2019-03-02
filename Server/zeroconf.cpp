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

#include "../Common/compiler.hpp"
#include "../Common/inline.hpp"
#include "output.hpp"
#include "zeroconf.hpp"
#include "config.hpp"

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <climits>
#endif

#if defined(Q_OS_LINUX) && defined(ZEROCONF_FOUND)
#define AVAHI_ZEROCONF
extern "C" {
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>
}
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

namespace {
Zeroconfig *instance = nullptr;
quint16 sip_port = 5060;
bool active = false;
bool started = false;
UString hostName = "_sipwitchqt.local";
UString srvName = "sipwitchqt";

#ifdef AVAHI_ZEROCONF
AvahiThreadedPoll *poller = nullptr;
AvahiClient *client = nullptr;
AvahiEntryGroup *srvGroup = nullptr;
AvahiEntryGroup *hostGroup = nullptr;
AvahiClientState clientState = AVAHI_CLIENT_S_REGISTERING;

void client_running();

void client_callback(AvahiClient *cbClient, AvahiClientState cbState, void *userData)
{
    Q_UNUSED(userData);
    if(!cbClient)
        return;

    client = cbClient;
    clientState = cbState;
    switch(cbState) {
    case AVAHI_CLIENT_S_RUNNING:
        client_running();
        break;
    case AVAHI_CLIENT_FAILURE:
        error() << "Zeroconfig failed; error=" << avahi_strerror(avahi_client_errno(client));
        break;
    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_S_REGISTERING:
        if(srvGroup)
            avahi_entry_group_reset(srvGroup);
        break;
    default:
        break;
    }
}

void srvGroup_callback(AvahiEntryGroup *cbGroup, AvahiEntryGroupState cbState, void *userData)
{
    char *altName;
    Q_UNUSED(cbGroup);
    Q_UNUSED(userData);

    switch(cbState) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        notice() << "Zeroconfig " << srvName << " service established";
        break;
    case AVAHI_ENTRY_GROUP_COLLISION:
        altName = avahi_alternative_service_name(srvName.constData());
        notice() << "Zeroconfig " << srvName.constData() << " service renamed " << altName;
        srvName = altName;
        client_running();
        break;
    case AVAHI_ENTRY_GROUP_FAILURE:
        error() << "Zeroconfig service failure; error=" << avahi_strerror(avahi_client_errno(client));
        break;
    default:
        break;
    }
}

void hostGroup_callback(AvahiEntryGroup *cbGroup, AvahiEntryGroupState cbState, void *userData)
{
    char *altName;
    Q_UNUSED(cbGroup);
    Q_UNUSED(userData);

    switch(cbState) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        notice() << "Zeroconfig " << hostName.constData() << " hostname established";
        break;
    case AVAHI_ENTRY_GROUP_COLLISION:
        altName = avahi_alternative_service_name(hostName.constData());
        notice() << "Zeroconfig " << hostName.constData() << " hostname renamed " << altName;
        hostName = altName;
        client_running();
        break;
    case AVAHI_ENTRY_GROUP_FAILURE:
        error() << "Zeroconfig service failure; error=" << avahi_strerror(avahi_client_errno(client));
        break;
    default:
        break;
    }
}

void client_running()
{
    if(!hostGroup)
        hostGroup = avahi_entry_group_new(client, hostGroup_callback, nullptr);
    if(!hostGroup) {
        error() << "Zeroconfig failure; error=" << avahi_strerror(avahi_client_errno(client));
        return;
    }
    if(!srvGroup)
        srvGroup = avahi_entry_group_new(client, srvGroup_callback, nullptr);
    if(!srvGroup) {
        error() << "Zeroconfig failure; error=" << avahi_strerror(avahi_client_errno(client));
        return;
    }
    if(avahi_entry_group_is_empty(hostGroup)) {
        notice() << "Zeroconfig creating host group";
        char localName[HOST_NAME_MAX] = ".";
        if(gethostname(&localName[1], sizeof(localName)-1) < 0) {
            error() << "Zeroconfig failure; cannot determine hostname";
            return;
        }
        strncat(localName, ".local", sizeof(localName) - strlen(localName) - 1);
        localName[sizeof(localName) - 1] = '\0';

        auto len = strlen(localName) - 1;
        char count = 0;
        for(int ind = static_cast<int>(len); ind >= 0; ind--) {
            if(localName[ind] == '.') {
                localName[ind] = count;
                count = 0;
            }
            else
               count++;
        }

        auto result = avahi_entry_group_add_record(hostGroup, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, static_cast<AvahiPublishFlags>(AVAHI_PUBLISH_USE_MULTICAST|AVAHI_PUBLISH_ALLOW_MULTIPLE), hostName.constData(), AVAHI_DNS_CLASS_IN, AVAHI_DNS_TYPE_CNAME, AVAHI_DEFAULT_TTL, localName, len + 2);
        if(result >= 0)
            result = avahi_entry_group_commit(hostGroup);
        if(result < 0)
            error() << "Zeroconfig; failed to update, error=" << avahi_strerror(result);
    }
    notice() << "Zeroconfig adding sip apply config port " << sip_port;
    auto result = avahi_entry_group_add_service(srvGroup, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
        static_cast<AvahiPublishFlags>(0), srvName.constData(), "_sip._udp", nullptr, nullptr, sip_port,
        "type=sipwitch", nullptr);
    if(result >= 0)
        result = avahi_entry_group_commit(srvGroup);
    if(result < 0)
        error() << "Zeroconfig; failed to update, error=" << avahi_strerror(result);
}
#endif
} // namespace

Zeroconfig::Zeroconfig(Server *server, quint16 port)
{
    Q_ASSERT(instance == nullptr);
    sip_port = port;
    if(sip_port) {  // 0 disables zeroconfig...
        connect(server, &Server::changeConfig, this, &Zeroconfig::applyConfig);
        connect(server, &Server::started, this, &Zeroconfig::onStartup);
        connect(server, &Server::finished, this, &Zeroconfig::onShutdown);
        active = true;
    }
    else
        notice() << "Zeroconfig disabled";
}

bool Zeroconfig::enabled()
{
    return active;
}

UString Zeroconfig::currentHostName()
{
    return hostName;
}

void Zeroconfig::applyConfig(const QVariantHash& config)
{
#ifdef AVAHI_ZEROCONF
    notice() << "Zeroconfig configuration...";
    if(started && active)
        onShutdown();
    auto namelist = config["localnames"].toStringList();
    if(!Util::isEmpty(namelist))
        active = false;
    else if(sip_port)
        active = true;
    if(started && active)
        onStartup();
#else
    Q_UNUSED(config);
#endif
}

void Zeroconfig::onStartup()
{
    started = true;
    if(!active)
        return;

#ifdef AVAHI_ZEROCONF
    int errorCode;
    notice() << "Zeroconfig starting up...";
    poller = avahi_threaded_poll_new();
    if(!poller) {
        error() << "Zeroconfig failed to start";
        return;
    }
    client = avahi_client_new(avahi_threaded_poll_get(poller),
        static_cast<AvahiClientFlags>(0), client_callback, nullptr, &errorCode);
    avahi_threaded_poll_start(poller);
#endif
}

void Zeroconfig::onShutdown()
{
#ifdef AVAHI_ZEROCONF
    notice() << "Zeroconfig shutting down...";
    if(poller)
        avahi_threaded_poll_stop(poller);
    if(client)
        avahi_client_free(client);
    client = nullptr;
    poller = nullptr;
#endif
}
