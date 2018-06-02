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

#ifndef INVITE_HPP_
#define INVITE_HPP_

#include "../Common/compiler.hpp"
#include "provider.hpp"

class Invite;
class RemoteSegment;

// basic invite processing segment can be for remote or local...
class Segment
{
    friend class Invite;
    Q_DISABLE_COPY(Segment)

public:
    bool update(const Event *event);

    static QList<RemoteSegment *>peers();

protected:
    Invite *invite;
    Context *context;
    int cid, did, tid;

    Segment(int callid, Invite *ip, Context *ctx);
    virtual ~Segment();
};

class LocalSegment final : public Segment 
{
    Q_DISABLE_COPY(LocalSegment)

public:
    LocalSegment(int callid, Invite *ip, Registry *rp);

private:
    const Registry *registry;
};

class RemoteSegment final : public Segment 
{
    Q_DISABLE_COPY(RemoteSegment)

public:
    RemoteSegment(int callid, Invite *ip, Provider *prov);   // standard provider
    RemoteSegment(int callid, Invite *ip, Context *ctx);     // p2p calls...
    ~RemoteSegment() final;

private:
    const Provider *provider;
};

class Invite
{
    Q_DISABLE_COPY(Invite)

public:
    Invite(const UString& id);
    ~Invite();

    inline Segment *origin() const {
        return from;
    }

    inline const UString display() const {
        return text;
    }

    static QList<Invite*> list();

private:
    Segment *from;
    QList<Segment*> to;
    UString tag;
    UString text;
};

QDebug operator<<(QDebug dbg, const Segment& seg);
QDebug operator<<(QDebug dbg, const Invite& cr);

/*!
 * These classes are used by the stack to keep track of active invites.
 * Each active invite is handled by a master object which has a segment for
 * each invited endpoint.  Because invite segments are handled differently for
 * locally managed devices and remote (provider or p2p) connections, the
 * segment is split into a base and derived local and remote classes.
 * \file invite.hpp
 */

/*!
 * \class RemoteSegment
 * \brief An invitation with a remote endpoint.
 * This represents a active invitation tied to an endpoint not associated with our server.
 * Typically this is for endpoints being reached by a provider, or in peer calling.
 * It could also happen if a dialed destination represents a uri of an actual endpoint
 * that is not registered with us.
 * \author David Sugar <tychosoft@gmail.com>
 */

/*!
 * \class LocalSegment
 * \brief An invitation with a local endpoint.
 * This represents an active invitation tied to an endpoint registered with SipWitchQt.
 * \author David Sugar <tychosoft@gmail.com>
 */

/*!
 * \class Segment
 * \brief An active invitation for a source or  destination.
 * A base class to represent an active invitation, whether local or remote.  A few segment
 * related utility functions may also be exposed from here.
 * \author David Sugar <tychosoft@gmail.com>
 *
 * \fn bool Segment::update(const Event *event)
 * \param event Event object we are updating from.
 * This is used to set the segments tid and cid from an associated eXosip event.
 * When creating an invite for a destination, we may have towait for the reply to
 * get these.  When receiving an invite for a destination, we can use the event
 * we received immediately.
 * \return false if we cannot apply the event to our segment.
 */

/*!
 * \class Invite
 * \brief An invitation between a source and destinations.
 * An initial invitation is offered from a source, and one or more destinations are then
 * invited.  The first destination segment that answers makes the call active, and
 * the remaining destinations are canceled.  The final bye is routed back to us so
 * that we can track call termination.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
