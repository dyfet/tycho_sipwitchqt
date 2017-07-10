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

#include "compiler.hpp"
#include "address.hpp"
#include "context.hpp"

#include <QSqlRecord>
#include <QElapsedTimer>

class LocalSegment;
class Registry;
class Event;

class Endpoint final
{
    friend class Registry;
    Q_DISABLE_COPY(Endpoint)

public:
    Endpoint(Context *ctx, const Address& addr, int expires, Registry *reg = nullptr);
    ~Endpoint();

    inline bool hasExpired() const {
        return updated.hasExpired(expiration);
    }

    inline const QHostAddress host() const {
        return address.host();
    }

    inline quint16 port() const {
        return address.port();
    }

    inline Context *sip() const {
        return context;
    }

    inline Registry *parent() const {
        return registry;
    }
    
    int expires(void) const;
    void refresh(int expires);
    
    static Endpoint *find(const Context *ctx, const Address& addr);

private:
    Registry *registry;             // registry that holds our endpoint
    Context *context;               // context endpoint exists on
    Address address;                // network address of endpoint
    QElapsedTimer updated;          // last refreshed registration
    qint64 expiration;              // msecs to expiration
    QList<LocalSegment *> calls;    // local calls on this endpoint...
};

class Registry final
{
    Q_DISABLE_COPY(Registry)

public:
    Registry(const QSqlRecord& db);
    ~Registry();

    inline const QSqlRecord data() const {
        return extension;
    }

    const QString display() const {
        return text;
    }

    bool hasExpired() const;

    int expires() const;

    static Registry *find(const QString& target);

    static QList<Registry *> list();

    static void events(const Event& event);

private:
    QSqlRecord extension;
    QString id, alias, text;

    QList<Endpoint*> endpoints;         // endpoint nodes
};

QDebug operator<<(QDebug dbg, const Endpoint& endpoint);
QDebug operator<<(QDebug dbg, const Registry& registry);

/*!
 * These classes are used to manage registration of sip device endpoints.
 * This includes a master registration object, and a separate endpoint
 * object associated with each physical sip user agent.  Multiple devices can
 * register under the same id.
 * \file registry.hpp
 * \ingroup Stack
 */

/*!
 * \class Endpoint
 * \brief A registration endpoint.
 * This represents a single SIP device that is registered thru the stack under
 * a registry object.
 * \author David Sugar <tychosoft@gmail.com>
 * \ingroup Stack
 */

/*!
 * \class Registry
 * \brief An active registration.
 * A registration consists of a user, and all the endpoints that are
 * registered thru the stack which are associated with that user.
 * \author David Sugar <tychosoft@gmail.com>
 * \ingroup Stack
 */
