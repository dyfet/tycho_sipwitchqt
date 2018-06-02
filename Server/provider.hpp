/*
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

#ifndef PROVIDER_HPP_
#define PROVIDER_HPP_

#include "registry.hpp"

class RemoteSegment;

class Provider final
{
    Q_DISABLE_COPY(Provider)

public:
    Provider(const QSqlRecord& db, Context *ctx, int rid = -1);
    ~Provider();

    inline const QSqlRecord data() const {
        return provider;
    }

    inline Context *sip() const {
        return context;
    }

    inline const UString display() const {
        return text;
    }

    static void reload(const QList<QSqlRecord>& records);
    static void update(const QSqlRecord& record);

    static Provider *find(const UString &target);

    static Provider *find(int rid);

    static QList<Provider *> list();

private:
    QSqlRecord provider;
    UString uri, text;
    Context *context;                   // to be set based on uri...
    int id;                             // exosip registration id

    QList<RemoteSegment*> calls;        // calls on this provider
};

QDebug operator<<(QDebug dbg, const Provider& prov);

/*!
 * This is used to manage instances of external sip providers.  This will
 * later include routing objects and rules which may be associated with a
 * provider.
 * \file provider.hpp
 */

/*!
 * \class Provider
 * \brief A remote provider instance.
 * A provider may be a sip trunking provider.  This should not be confused
 * with locally managed gateways, which will be handled thru the registry.
 * This provides contact information and external registration management.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
