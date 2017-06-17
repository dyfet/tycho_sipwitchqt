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

/*!
 * Manage information on internet subnets.
 * \file subnet.hpp
 */

#include "compiler.hpp"
#include <QString>
#include <QByteArray>
#include <QHostAddress>
#include <QPair>
#include <QAbstractSocket>

/*!
 * \brief Internet subnet/cidr class.
 * This holds the base address and network mask for a subnet.  This allows
 * us to maintain list of valid subnets, and to determine if a given host
 * name happens to fall inside a subnet.
 * \author David Sugar <tychosoft@gmail.com>
 */
class Subnet
{
public:
    Subnet(QHostAddress addr, int mask) noexcept;

	Subnet(const QString& addr) noexcept;

    Subnet() noexcept;

    Subnet(const Subnet& from) noexcept;

    Subnet(Subnet&& from) noexcept;

    Subnet& operator=(const Subnet& from) {
        pair = from.pair;
        return *this;
    }

    Subnet& operator=(Subnet&& from) {
        pair = std::move(from.pair);
        from.pair.second = 0;
        from.pair.first = QHostAddress::Any;
        return *this;
    }

    bool operator==(const Subnet& other) const {
        return pair == other.pair;
    }

    bool operator!=(const Subnet& other) const {
        return pair != other.pair;
    }

    inline operator QPair<QHostAddress,int>() {
        return pair;
    }

	inline const QHostAddress address() const {
        return pair.first;
	}

	inline int mask() const {
        return pair.second;
	}

	inline QAbstractSocket::NetworkLayerProtocol protocol() const {
        return pair.first.protocol();
	}

    inline bool contains(const QHostAddress& host) const {
        return host.isInSubnet(pair);
    }

    inline operator bool() const {
        return pair.second > 0;
    }

    inline bool operator!() const {
        return pair.second == 0;
    }

private:
    QPair<QHostAddress,int> pair;

    friend uint qHash(const Subnet& key, uint seed) {
        return qHash(key.pair.first, seed) ^ key.mask();
    }

};

QDebug operator<<(QDebug dbg, const Subnet& cidr);
