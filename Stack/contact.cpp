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

#include <Common/inline.hpp>
#include "contact.hpp"

Contact::Contact(const QString& address, quint16 port, const QString& user, int duration) noexcept :
userName(user), expiration(0)
{
    hostName = address;
    hostPort = port;
    if(duration > -1) {
        time(&expiration);
        expiration += duration;
    }
}

Contact::Contact(osip_uri_t *uri)  noexcept :
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
        hostPort = atoi(uri->port);
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
    osip_contact_param_get_byname(contact, (char *)"expires", &param);
    if(param && param->gvalue)
        refresh(osip_atoi(param->gvalue));

    hostName = uri->host;
    if(uri->scheme && !strcmp(uri->scheme, "sips"))
        hostPort = 5061;
    else
        hostPort = 5060;
    userName = uri->username;
    if(uri->port && uri->port[0])
        hostPort = atoi(uri->port);
 }

Contact::Contact(const Contact& from) noexcept :
hostName(from.hostName), hostPort(from.hostPort), userName(from.userName), expiration(from.expiration)
{
}

Contact::Contact() noexcept :
hostPort(0), expiration(0)
{
}

Contact& Contact::operator=(const Contact& from) {
    hostName = from.hostName;
    hostPort = from.hostPort;
    userName = from.userName;
    expiration = from.expiration;
    return *this;
}

const QString Contact::toString() const {
    if(!hostPort)
        return "invalid";
    QString port = ":" + QString::number(hostPort);
    if(hostName.contains(":") && hostName[0] != '[')
        return "[" + hostName + "]" + port;
    return hostName + port;
}

void Contact::clear() {
    hostPort = 0;
    hostName.clear();
    userName.clear();
    expiration = 0;
}

bool Contact::hasExpired() const {
    if(!expiration)
        return false;
    time_t now;
    time(&now);
    if(now >= expiration)
        return true;
    return false;
}

void Contact::refresh(int seconds) {
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
