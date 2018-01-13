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

#include "invite.hpp"

static QHash<UString,Invite*> invites;
static QHash<int,Segment*> segments;
static QList<RemoteSegment*> peering;

Invite::Invite(const UString id) :
tag(id)
{
    invites.insert(tag, this);
}

Invite::~Invite()
{
    invites.remove(tag);
}

QList<Invite*> Invite::list()
{
    return invites.values();
}

Segment::Segment(int callid, Invite *ip, Context *ctx) :
invite(ip), context(ctx), cid(callid), did(-1), tid(-1)
{
    segments.insert(cid, this);
}

Segment::~Segment()
{
    segments.remove(cid);
}

bool Segment::update(const Event *event)
{
    if(event->cid() != cid || context != event->context())
        return false;

    if(did == -1)
        did = event->did();

    if(tid == -1)
        tid = event->tid();

    return true;
}

QList <RemoteSegment *> Segment::peers()
{
    return peering;
}        

LocalSegment::LocalSegment(int cid, Invite *ip, Registry *rp) :
Segment(cid, ip, rp->sip()), registry(rp)
{
}

RemoteSegment::RemoteSegment(int cid, Invite *ip, Provider *pp) :
Segment(cid, ip, pp->sip()), provider(pp)
{
}

RemoteSegment::RemoteSegment(int cid, Invite *ip, Context *ctx) :
Segment(cid, ip, ctx), provider(nullptr)
{
    peering.append(this);
}

RemoteSegment::~RemoteSegment()
{
    if(!provider)
        peering.removeOne(this);
}
