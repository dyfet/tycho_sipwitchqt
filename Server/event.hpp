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

#ifndef EVENT_HPP_
#define EVENT_HPP_

#include "../Common/compiler.hpp"
#include "../Common/util.hpp"
#include "../Common/contact.hpp"

#include <QThread>
#include <QMutex>
#include <QHostAddress>
#include <QHostInfo>
#include <QAbstractSocket>
#include <QSharedData>
#include <QElapsedTimer>

class Context;

class Event final
{
public:
    Event();
    Event(eXosip_event_t *evt, Context *ctx);
    Event(const Event& copy);
    ~Event();

    inline operator bool() const {
        return d->event != nullptr;
    }

    inline bool operator!() const {
        return d->event == nullptr;
    }

    inline const eXosip_event_t *event() const {
        Q_ASSERT(d->event != nullptr);
		return d->event;
	}

    inline eXosip_event_type_t type() const {
        Q_ASSERT(d->event != nullptr);
        return d->event->type;
    }

    inline Context *context() const {
        Q_ASSERT(d->event != nullptr);
        return d->context;
    }

    inline const QList<Contact>& contacts() const {
        return d->contacts;
    }

    inline const QList<Contact>& routes() const {
        return d->routes;
    }

    inline int expires() const {
        return d->expires;
    }

    inline int status() const {
        return d->status;
    }

    inline const UString agent() const {
        return d->agent;
    }

    inline const UString reason() const {
        return d->reason;
    }

    inline const UString method() const {
        return d->method;
    }

    inline int hops() const {
        return d->hops;
    }

    inline bool record() const {
        return d->record;
    }

    inline const Contact source() const {
        return d->source;
    }

    inline const UString authorizingId() const {
        return d->userid;
    }

    inline const UString authorizingDigest() const {
        return d->digest;
    }

    inline const UString authorizingOnce() const {
        return d->nonce;
    }

    inline const UString authorizingRealm() const {
        return d->realm;
    }

    inline const UString authorizingAlgorithm() const {
        return d->algorithm;
    }

    inline osip_authorization_t *authorization() const {
        return d->authorization;
    }

    inline int number() const {
        return d->number;
    }

    inline const Contact from() const {
        return d->from;
    }

    inline const Contact to() const {
        return d->to;
    }

    inline const UString request() const {
        return d->request;
    }

    inline const Contact target() const {
        return d->target;
    }

    inline const UString uriSource() const {
        return uri(d->source);
    }

    inline const UString uriTarget() const {
        return uri(d->target);
    }

    inline bool isNatted() const {
        return d->natted;
    }

    inline bool isLocal() const {
        return d->local;
    }

    inline bool isAssociated() const {
        return d->associated;
    }

    inline const osip_message_t* message() const {
        return d->message;
    }

    inline const QByteArray body() const {
        return d->body;
    }

    inline const UString content() const {
        return d->content;
    }

    inline const UString subject() const {
        return d->subject;
    }

    inline const UString initialize() const {
        return d->initialize;
    }

    inline int cid() const {
        Q_ASSERT(d->event != nullptr);
        return d->event->cid;
    }

    inline int did() const {
        Q_ASSERT(d->event != nullptr);
        return d->event->did;
    }

    inline int tid() const {
        Q_ASSERT(d->event != nullptr);
        return d->event->tid;
    }

    inline int rid() const {
        Q_ASSERT(d->event != nullptr);
        return d->event->rid;
    }

    inline qint64 elapsed() const {
        return d->elapsed.elapsed();
    }

    inline QList<UString> allows() const {
        return d->allows;
    }

    inline UString label() const {
        return d->label;
    }

    const UString protocol() const;
    const UString toString() const;
    const Contact contact() const;
    const UString uriContext(const UString& username = UString()) const;
    const UString text() const;
    const UString uri(const Contact &addr) const;

private:
    class Data final : public QSharedData
	{
        Q_DISABLE_COPY(Data)        // can never deep copy...
	public:
        Data();
        Data(eXosip_event_t *evt, Context *ctx);
        ~Data();

        int number;                 // referencing extension # or -1
        int expires;                // longest expiration
        int status;
        int hops;                   // via hops
        bool natted, local, associated, record;
        Context *context;
        eXosip_event_t *event;
        osip_message_t *message;
        osip_authorization_t *authorization;
        QList<Contact> contacts, routes;
        UString agent, method, subject, text, content, realm, reason, initialize;
        UString userid, nonce, digest, algorithm, label, request;
        Contact source;  // if nat, has first nat
        Contact from, to, target;
        QList<UString> allows;
        QByteArray body;
        QElapsedTimer elapsed;

        void parseMessage(osip_message_t *msg);
    };

    QSharedDataPointer<Event::Data> d;
};

QDebug operator<<(QDebug dbg, const Event& evt);

Q_DECLARE_METATYPE(Event)

/*!
 * Define sip event object that can be passed from Context to Stack.
 * This is used to define the core event system we use between
 * context stacks and signaled sip events sent to the main
 * stack manager.
 * \file event.hpp
 */

/*!
 * \class Event
 * \brief Container for SIP Events.
 * Each SIP event generates an implicitly shared SIP event object.  This
 * object contains eXosip2 and context related event data structures which
 * can then be passed to the main sip Stack class and handled in virtual
 * slots.  Exosip2 memory management for these data structures will also
 * be buried inside here.
 *
 * A lot of computation and parsing for lists, to determine nat and source
 * address, contacts, etc, happens in the constructor automatically for 
 * every event.  The event object is constructed in each Context thread 
 * before handed off; this maximizes compute in parallel listeners rather
 * than in bottlenecks like the stack manager thread.
 * \author David Sugar <tychosoft@gmail.com>
 *
 * \fn Event::contacts()
 * Returns list of parsed contacts.  Register and 3xx responses can have
 * multipe contacts.
 *
 * \fn Event::contact()
 * Returns a single valid contact from the event.  If either no contacts,
 * or multiple contacts are present, then returns empty Contact.
 */

#endif
