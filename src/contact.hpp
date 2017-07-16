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

#include "compiler.hpp"
#include <QString>
#include <QByteArray>
#include <QHostAddress>
#include <QPair>
#include <QAbstractSocket>
#include <eXosip2/eXosip.h>

class Contact final
{
public:
    Contact(const QString& address, quint16 port, const QString& user = "", int duration = -1) noexcept;

    Contact(osip_contact_t *contact) noexcept;

    Contact(osip_uri_t *uri) noexcept;

    Contact() noexcept;

    Contact(const Contact& from) noexcept;

    Contact(Contact&& from) noexcept;

    Contact& operator=(const Contact& from) {
        pair = from.pair;
        expiration = from.expiration;
        username = from.username;
        return *this;
    }

    Contact& operator=(Contact&& from) {
        pair = std::move(from.pair);
        from.pair.second = 0;
        from.pair.first = "";
        from.expiration = 0;
        from.username = "";
        return *this;
    }

    operator bool() const {
        return pair.second != 0;
    }

    bool operator!() const {
        return pair.second == 0;
    }

    bool operator==(const Contact& other) const {
        return pair == other.pair && username == other.username;
    }

    bool operator!=(const Contact& other) const {
        return pair != other.pair || username != other.username;
    }

    time_t expires() const {
        return expiration;
    }

    void refresh(const Contact& from) {
        if(from != *this)
            return;

        expiration = from.expiration;
    }

    bool hasUserId() const {
        return username.length() > 0;
    }

    const QString userId() const {
        return username;
    }

    const QHostAddress address() const {
        if(pair.second)
            return QHostAddress(pair.first);
        else
            return QHostAddress();
    }

    const QString host() const {
        return pair.first;
    }

    quint16 port() const {
        return pair.second;
    }

    bool hasExpired() const;
    const QString toString() const;

    void refresh(int seconds);

private:
    QPair<QString,quint16> pair;
    time_t expiration;
    QString username;

private:
    friend uint qHash(const Contact& key, uint seed) {
        return qHash(key.username) ^ qHash(key.pair.first, seed) ^ key.port();
    }
};

QDebug operator<<(QDebug dbg, const Contact& contact);

/*!
 * Manage information on internet connections.
 * \file address.hpp
 * \ingroup Stack
 */

/*!
 * \class Contact
 * \brief A SIP endpoint contact.
 * This provides a convenient means to represent a sip endpoint address
 * along with it's expected duration and userid.  This uses strings for
 * host address so that the actual dns resolution happens in the eXosip2
 * library using the c-ares resolver.
 * \author David Sugar <tychosoft@gmail.com>
 * \ingroup Stack
 */
