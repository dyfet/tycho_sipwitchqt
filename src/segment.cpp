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

#include "call.hpp"

static QHash<int,Segment*> calls;
static QList<RemoteSegment*> peering;

Segment::Segment(int cid, Call *cp, Context *ctx) :
call(cp), context(ctx), id(cid)
{
    calls.insert(id, this);
}

Segment::~Segment()
{
    calls.remove(id);
}

QList <RemoteSegment *> Segment::peers()
{
    return peering;
}        

LocalSegment::LocalSegment(int cid, Call *cp, Endpoint *ep) :
Segment(cid, cp, ep->sip()), endpoint(ep), registry(ep->parent())
{
}

RemoteSegment::RemoteSegment(int cid, Call *cp, Provider *pp) :
Segment(cid, cp, pp->sip()), provider(pp)
{
}

RemoteSegment::RemoteSegment(int cid, Call *cp, Context *ctx) :
Segment(cid, cp, ctx), provider(nullptr)
{
    peering.append(this);
}

RemoteSegment::~RemoteSegment()
{
    if(!provider)
        peering.removeOne(this);
}
