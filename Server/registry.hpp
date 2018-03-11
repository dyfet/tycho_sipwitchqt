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
    Registry(const QVariantHash& ep);
    ~Registry();

    const UString display() const {
        return userDisplay;
    }

    const UString user() const {
        return userId;
    }

    const UString realm() const {
        return authRealm;
    }

    const UString digest() const {
        return authDigest;
    }

    const UString label() const {
        return userLabel;
    }

    int extension() const {
        return number;
    }

    int expires() const {
        return static_cast<int>(timeout / 1000l);
    }

    inline const UString host() const {
        return address.host();
    }

    inline const UString origin() const {
        return userOrigin;
    }

    inline quint16 port() const {
        return address.port();
    }

    inline const UString route() const {
        return address.toString();
    }

    inline Context *context() const {
        return serverContext;
    }

    inline void refresh(int expires) {
        address.refresh(expires);
    }

    inline bool allow(UString id) const {
        return allows.indexOf(id.toLower()) > -1;
    }

    bool hasExpired() const {
        return updated.hasExpired(timeout);
    }

    bool isActive() const {
        return serverContext != nullptr;
    }

    bool isLabeled() const {
        return !userLabel.isEmpty() && userLabel != "NONE";
    }

    QByteArray nounce() const {
        return random;
    }

    void setNounce(const QByteArray& value) {
        prior = random;
        random = value;
    }

    int authorize(const Event& event);
    int authenticate(const Event& event);

    static Registry *find(const Event& event);      // to find registration
    static Registry *find(qlonglong);
    static QList<Registry *> find(const UString& target);
    static QList<Registry *> list();
    static UString bitmask();

    static void cleanup();

private:
    UString userId, userLabel, userSecret, authRealm, authDigest;
    UString userDisplay, userAgent, userOrigin;
    int number, rid;
    qlonglong endpointId;               // endpoint id from database
    qint64 timeout;                     // time till expires
    QByteArray random, prior;           // nounce value
    Context *serverContext;             // context of endpoint
    Contact address;                    // contact record for endpoint
    QElapsedTimer updated;              // when the record was updated
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
