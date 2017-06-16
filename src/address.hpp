/**
 ** Copyright 2017 Tycho Softworks.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

/**
 * Manage information on internet connections.
 * @file address.hpp
 */

#include "compiler.hpp"
#include <QString>
#include <QByteArray>
#include <QHostAddress>
#include <QPair>
#include <QAbstractSocket>

/**
 * @brief An internet connection address.
 * This provides a convenient means to represent an internet connection
 * as a host address and port number.
 * @author David Sugar <tychosoft@gmail.com>
 */
class Address
{
public:
    Address(QHostAddress addr, quint16 port) noexcept;

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
        from.pair.first = QHostAddress::Any;
        return *this;
    }

    bool operator==(const Address& other) const {
        return pair == other.pair;
    }

    bool operator!=(const Address& other) const {
        return pair != other.pair;
    }

    inline operator QPair<QHostAddress,quint16>() {
        return pair;
    }

    inline const QHostAddress host() const {
        return pair.first;
	}

    inline quint16 port() const {
        return pair.second;
	}

	inline QAbstractSocket::NetworkLayerProtocol protocol() const {
        return pair.first.protocol();
	}

    inline operator bool() const {
        return pair.second > 0;
    }

    inline bool operator!() const {
        return pair.second == 0;
    }

private:
    QPair<QHostAddress,quint16> pair;

    friend uint qHash(const Address& key, uint seed) {
        return qHash(key.pair.first, seed) ^ key.port();
    }

};

QDebug operator<<(QDebug dbg, const Address& addr);
