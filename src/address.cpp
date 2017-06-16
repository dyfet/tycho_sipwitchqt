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

#include "inline.hpp"
#include "address.hpp"

Address::Address(QHostAddress host, quint16 port) noexcept :
pair(QPair<QHostAddress,int>(host, port))
{
}

Address::Address(const Address& from) noexcept :
pair(from.pair)
{
}

Address::Address(Address&& from) noexcept :
pair(std::move(from.pair))
{
    from.pair.second = 0;
    from.pair.first = QHostAddress::Any;
}

Address::Address() noexcept :
pair(QHostAddress::Any, 0)
{
}

QDebug operator<<(QDebug dbg, const Address& addr)
{
    dbg.nospace() << "Address(" << addr.host() << ":" << addr.port() << ")";
    return dbg.maybeSpace();
}


