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

#include "../Common/inline.hpp"
#include "../Common/util.hpp"
#include "contact.hpp"

Contact::Contact(const UString& address, quint16 port, const UString& user, int duration) noexcept
{
    userName = user;
    hostName = address;
    hostPort = port;
    expiration = 0;

    if(duration > -1) {
        time(&expiration);
        expiration += duration;
    }
}

Contact::Contact(const QString& uri, QString server) noexcept :
hostPort(0), expiration(0)
{
    int lead = 0;

    if(uri.left(4).toLower() == "sip:") {
        hostPort = 5060;
        lead = 4;
    }
    else if(uri.left(5).toLower() == "sips:") {
        hostPort = 5061;
        lead = 5;
    }

    int pos = uri.indexOf("@");
    if(pos > 0) {
        userName = uri.mid(lead, pos - lead);
        server = uri.mid(pos + 1);
    } else {
        userName = uri.mid(lead);
        if(server.left(4).toLower() == "sip:") {
            server = server.mid(4);
            if(!hostPort)
                hostPort = 5060;
        }
        else if(server.left(5).toLower() == "sips:") {
            server = server.mid(5);
            if(!hostPort)
                hostPort = 5061;
        }
    }
    lead = server.indexOf("[");
    if(lead < 1)
        lead = 0;

    if(server.mid(lead).indexOf(":") == server.lastIndexOf(":"))
    {
        if(lead > 0)
            hostName = server.left(lead);
        else
            hostName = server.left(server.indexOf(":"));
        hostPort = static_cast<quint16>(server.mid(server.lastIndexOf(":") + 1).toInt());
    }
    else
        hostName = server;

    if(!hostPort)
        hostPort = 5060;
}

Contact::Contact(const osip_uri_t *uri)  noexcept :
hostPort(0), expiration(0)
{
    if(!uri || !uri->host || uri->host[0] == 0)
        return;

    hostName = uri->host;
    if(uri->scheme && !strcmp(uri->scheme, "sips"))
        hostPort = 5061;
    else
        hostPort = 5060;
    userName = uri->username;
    if(uri->port && uri->port[0])
        hostPort = Util::portNumber(uri->port);
}

Contact::Contact(osip_contact_t *contact)  noexcept :
hostPort(0), expiration(0)
{
    if(!contact || !contact->url)
        return;

    osip_uri_t *uri = contact->url;
    if(!uri->host || uri->host[0] == 0)
        return;

    // see if contact has expiration
    osip_uri_param_t *param = nullptr;
    osip_contact_param_get_byname(contact, const_cast<char *>("expires"), &param);
    if(param && param->gvalue)
        refresh(osip_atoi(param->gvalue));

    hostName = uri->host;
    if(uri->scheme && !strcmp(uri->scheme, "sips"))
        hostPort = 5061;
    else
        hostPort = 5060;
    userName = uri->username;
    if(uri->port && uri->port[0])
        hostPort = Util::portNumber(uri->port);
}

Contact::Contact() noexcept :
hostPort(0), expiration(0)
{
}

const UString Contact::toString() const
{
    if(!hostPort)
        return "invalid";
    UString port = ":" + UString::number(hostPort);
    if(hostName.contains(":") && hostName[0] != '[')
        return hostName.quote("[]") + port;
    return hostName + port;
}

void Contact::clear()
{
    hostPort = 0;
    hostName.clear();
    userName.clear();
    expiration = 0;
}

bool Contact::hasExpired() const
{
    if(!expiration)
        return false;
    time_t now;
    time(&now);
    return now >= expiration;
}

void Contact::refresh(int seconds)
{
    if(seconds < 0) {
        expiration = 0;
        return;
    }

    if(!expiration)
        time(&expiration);
    expiration += seconds;
}

QDebug operator<<(QDebug dbg, const Contact& addr)
{
    time_t now, expireTime = addr.expires();
    time(&now);
    QString expires = "never";
    if(expireTime) {
        if(now >= expireTime)
            expires = "expired";
        else
            expires = QString::number(expireTime - now) + "s";
    }
    dbg.nospace() << "Contact(" << addr.host() << ":" << addr.port() << ",expires=" << expires << ")";
    return dbg.maybeSpace();
}
