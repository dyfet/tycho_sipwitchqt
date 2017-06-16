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
#include "subnet.hpp"

Subnet::Subnet(QHostAddress host, int mask) noexcept :
pair(QPair<QHostAddress,int>(host, mask))
{
}

Subnet::Subnet(const QString& addr) noexcept :
pair(QPair<QHostAddress,int>(QHostAddress::parseSubnet(addr)))
{
}

Subnet::Subnet(const Subnet& from) noexcept :
pair(from.pair)
{
}

Subnet::Subnet(Subnet&& from) noexcept :
pair(std::move(from.pair))
{
    from.pair.second = 0;
    from.pair.first = QHostAddress::Any;
}

Subnet::Subnet() noexcept :
pair(QHostAddress::Any, 0)
{
}

QDebug operator<<(QDebug dbg, const Subnet& cidr)
{
    dbg.nospace() << "Subnet(" << cidr.address() << "/" << cidr.mask() << ")";
    return dbg.maybeSpace();
}


