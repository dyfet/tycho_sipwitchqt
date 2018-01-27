/**
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
    Contact(const UString& address, quint16 port, const UString& user = UString(), int duration = -1) noexcept;
    Contact(const QString& uri, QString server = QString()) noexcept;
    Contact(osip_contact_t *contact) noexcept;
    Contact(osip_uri_t *uri) noexcept;
    Contact() noexcept;
    Contact(const Contact& from) noexcept;

    Contact& operator+=(const Contact& from) {
        refresh(from);
        return *this;
    }

    Contact& operator=(const Contact& from);

    operator bool() const {
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
        if(from == *this && from.expiration > 0)
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
 */

/*!
 * \namespace Util
 */

#endif
