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

class Address
{
public:
    Address(const QString& address, quint16 port) noexcept;

    Address() noexcept;

    Address(const Address& from) noexcept;

    Address(Address&& from) noexcept;

    Address& operator=(const Address& from) {
        pair = from.pair;
        return *this;
    }

    Address& operator=(Address&& from) {
        pair = std::move(from.pair);
        from.pair.second = 0;
        from.pair.first = "";
        return *this;
    }

    bool operator==(const Address& other) const {
        return pair == other.pair;
    }

    bool operator!=(const Address& other) const {
        return pair != other.pair;
    }

    inline const QString host() const {
        return pair.first;
	}

    inline quint16 port() const {
        return pair.second;
	}

    inline operator bool() const {
        return pair.second > 0;
    }

    inline bool operator!() const {
        return pair.second == 0;
    }

protected:
    QPair<QString,quint16> pair;

private:
    friend uint qHash(const Address& key, uint seed) {
        return qHash(key.pair.first, seed) ^ key.port();
    }

};

class Contact final : public Address
{
public:
    Contact(const QString& address, quint16 port, int duration = -1, const QString& user = "") noexcept;

    Contact(const Address& address, int duration = -1, const QString& user = "") noexcept;

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

    bool operator==(const Contact& other) const {
        return pair == other.pair && username == other.username;
    }

    bool operator!=(const Contact& other) const {
        return pair != other.pair || username != other.username;
    }

    time_t expires() const {
        return expiration;
    }

    void refresh(int seconds) {
        if(expiration)
            expiration += seconds;
    }

    QString userId() const {
        return username;
    }

    bool hasExpired() const;

private:
    time_t expiration;
    QString username;

private:
    friend uint qHash(const Contact& key, uint seed) {
        return qHash(key.username) ^ qHash(key.pair.first, seed) ^ key.port();
    }
};

QDebug operator<<(QDebug dbg, const Address& addr);
QDebug operator<<(QDebug dbg, const Contact& contact);

/*!
 * Manage information on internet connections.
 * \file address.hpp
 * \ingroup Stack
 */

/*!
 * \class Address
 * \brief A SIP endpoint address.
 * This provides a convenient means to represent a remote connection
 * as a host address and port number.  This uses strings so that the
 * actual host dns resolution happens in the eXosip2 library using c-ares
 * resolver on demand, rather than when the object is created.
 * \author David Sugar <tychosoft@gmail.com>
 * \ingroup Stack
 */

/*!
 * \class Contact
 * \brief A SIP endpoint contact.
 * This provides a convenient means to represent a sip endpoint address
 * along with it's expected duration.
 * \author David Sugar <tychosoft@gmail.com>
 * \ingroup Stack
 */
