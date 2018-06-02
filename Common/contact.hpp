/**
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

#ifndef CONTACT_HPP_
#define CONTACT_HPP_

#include "../Common/types.hpp"
#include <QString>
#include <QByteArray>
#include <QHostAddress>
#include <QPair>
#include <QAbstractSocket>
#include <eXosip2/eXosip.h>

#ifndef SIP_CONFLICT
#define SIP_CONFLICT    409         // we bring it back for duplicate labels!
#endif

class Contact final
{
public:
    Contact(const UString& address, quint16 port, const UString&  user = UString(), int duration = -1) noexcept;
    Contact(const QString& uri, QString server = QString()) noexcept;
    explicit Contact(osip_contact_t *contact) noexcept;
    Contact(const osip_uri_t *uri) noexcept;
    Contact() noexcept;
    Contact(const Contact& from) noexcept = default;

    Contact& operator+=(const Contact& from) {
        refresh(from);
        return *this;
    }

    Contact& operator=(const Contact& from) = default;

    explicit operator bool() const {
        return hostPort != 0 && !hostName.isEmpty();
    }

    bool operator!() const {
        return hostPort == 0 && !hostName.isEmpty();
    }

    bool operator==(const Contact& other) const {
        return hostName == other.hostName && hostPort == other.hostPort && userName == other.userName;
    }

    bool operator!=(const Contact& other) const {
        return hostName != other.hostName || hostPort != other.hostPort || userName != other.userName;
    }

    time_t expires() const {
        return expiration;
    }

    void refresh(const Contact& from) {
        if(from != *this && from.expiration > 0)
            expiration = from.expiration;
    }

    bool hasUser() const {
        return userName.length() > 0;
    }

    const UString user() const {
        return userName;
    }

    const UString host() const {
        return hostName;
    }

    quint16 port() const {
        return hostPort;
    }

    bool hasExpired() const;
    const UString toString() const;

    void clear();
    void refresh(int seconds);

private:
    UString hostName;
    quint16 hostPort;
    UString userName;
    time_t expiration;

private:
    friend uint qHash(const Contact& key, uint seed) {
        return qHash(key.userName, seed) ^ qHash(key.hostName, seed) ^ key.hostPort;
    }
};

QDebug operator<<(QDebug dbg, const Contact& contact);

/*!
 * Manage information on internet connections.
 * \file contact.hpp
 */

/*!
 * \class Contact
 * \brief A SIP endpoint contact.
 * This provides a convenient means to represent a sip endpoint address
 * along with it's expected duration and userid.  This uses strings for
 * host address so that the actual dns resolution happens in the eXosip2
 * library using the c-ares resolver.
 * \author David Sugar <tychosoft@gmail.com>
 *
 * \fn Contact::Contact(const UString& address, quint16 port, const UString& user, int duration)
 * Construct a uri contact for a designated endpoint and optional user identity.
 * \param address Endpoint ip address or hostname.
 * \param port Endpoint ip port number or 0 for default (5060).
 * \param user Identity or QString() if not associated with a user identity.
 * \param duration Expiration of contact, -1 for never expires.
 *
 * \fn Contact::Contact(const QString uri, QString server)
 * Converts a string user id or full uri into a sip contact object.
 * \param uri A partial or full sip uri (such as sip:id@server:port), or
 * just a user id.
 * \param server A server uri to apply if the uri is just a user id.
 *
 * \fn Contact::Contact(osip_contact_t *contact)
 * Converts an osip contact structure into a sip contact object.
 * \param contact A contact defined in osip library.
 *
 * \fn Contact::Contact(const osip_uri_t *uri)
 * Converts an osip uri into a sip contact object.
 * \param uri A uri defined in osio library.
 *
 * \fn Contact::Contact()
 * Create an empty contact object that points to no endpoint.
 *
 * \fn Contact::expires()
 * Time this contact expires.
 * \return time_t of time expected to expire.
 *
 * \fn Contact::refresh(const Contact& from)
 * Replace expiration time based on another contacts expiration.
 * \param from Contact to get expiration from.
 *
 * \fn Contact::refresh(int seconds)
 * Refresh by adding additional time to contact expiration.
 * \param seconds Number of seconds to add.
 *
 * \fn Contact::hasUser()
 * Test if contact endpoint includes a user id.
 * \return true if includes user.
 *
 * \fn Contact::hasExpired()
 * Test if contact has already expired.
 * \return true if contact expires and past expiration time.
 *
 * \fn Contact::clear()
 * Creates an empty contact record, removing all previous data.
 *
 * \fn Contact::user()
 * Get user identity associated with this contact endpoint.
 * \return Identity or empty string.
 *
 * \fn Contact::host()
 * Get host address/name associated with this contact endpoint.
 * \return Host or empty string if not set.
 *
 * \fn Contact::port()
 * Get port number associated with this contact endpoint.
 * \return internet port number for contact.
 */

/*!
 * \namespace Util
 */

#endif
