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

#ifndef REGISTRY_HPP_
#define REGISTRY_HPP_

#include "../Common/compiler.hpp"
#include "context.hpp"

#include <QSqlRecord>
#include <QElapsedTimer>

class LocalSegment;
class Registry;
class Event;

class Registry final
{
    Q_DISABLE_COPY(Registry)

public:
    Registry(const QSqlRecord& db, const Event& event);
    ~Registry();

    inline const QSqlRecord data() const {
        return extension;
    }

    const UString display() const {
        return text;
    }

    inline bool hasExpired() const {
        return address.hasExpired();
    }

    inline const UString host() const {
        return address.host();
    }

    inline quint16 port() const {
        return address.port();
    }

    inline Context *sip() const {
        return context;
    }

    inline time_t expires() const {
        return address.expires();
    }

    inline void refresh(int expires) {
        address.refresh(expires);
    }

    inline bool allow(UString id) const {
        return allows.indexOf(id.toLower()) > -1;
    }

    static Registry *find(const QSqlRecord& db);
    static QList<Registry *> find(const UString& target);
    static QList<Registry *> list();

    static void process(const Event& event);
    static void authorize(const Event& event);

private:
    UString alias, label;
    UString text, agent;
    int number, rid;
    Context *context;                   // context of endpoint
    Contact address;                    // contact record for endpoint
    Contact route;                      // our return route to endpoint
    QElapsedTimer updated;              // last refershed registration
    QSqlRecord extension;               // extension + group union
    QList<LocalSegment *> calls;        // local calls on this endpoint
    QList<UString> allows;
};

QDebug operator<<(QDebug dbg, const Registry& registry);

/*!
 * These classes are used to manage registration of sip device endpoints.
 * This includes a master registration object, and a separate endpoint
 * object associated with each physical sip user agent.  Multiple devices can
 * register under the same id.
 * \file registry.hpp
 */

/*!
 * \class Registry
 * \brief An active registration.
 * A registration consists of a user endpoints that is registered
 * thru the stack which are associated with that user.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
