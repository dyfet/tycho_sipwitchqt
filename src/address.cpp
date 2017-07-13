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

Address::Address(const QString& host, quint16 port) noexcept :
pair(QPair<QString,quint16>(host, port))
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
    from.pair.first = "";
}

Address::Address() noexcept :
pair("", 0)
{
}

Contact::Contact(const QString& host, quint16 port, int duration, const QString& user) noexcept :
Address(host, port), expiration(0), username(user)
{
    if(duration > -1) {
        time(&expiration);
        expiration += duration;
    }
}

Contact::Contact(const Address& address, int duration, const QString& user) noexcept :
Address(address), expiration(0), username(user)
{
    if(duration > -1) {
        time(&expiration);
        expiration += duration;
    }
}

Contact::Contact(const Contact& from) noexcept
{
    pair = from.pair;
    expiration = from.expiration;
    username = from.username;
}

Contact::Contact(Contact&& from) noexcept
{
    pair = std::move(from.pair);
    from.pair.second = 0;
    from.pair.first = "";
    from.expiration = 0;
    from.username = "";
}

Contact::Contact() noexcept :
Address(), expiration(0)
{
}

bool Contact::hasExpired() const {
    if(!expiration)
        return false;
    time_t now;
    time(&now);
    if(now >= expiration)
        return true;
}

QDebug operator<<(QDebug dbg, const Address& addr)
{
    dbg.nospace() << "Address(" << addr.host() << ":" << addr.port() << ")";
    return dbg.maybeSpace();
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
