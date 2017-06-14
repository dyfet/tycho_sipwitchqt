/**
 ** Copyright 2017 Tycho Softworks.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "segment.hpp"

static QHash<int,Segment*> calls;

Segment::Segment(int cid, const Call *cp, const Context *ctx) :
call(cp), context(ctx), id(cid)
{
    calls.insert(id, this);
}

Segment::~Segment()
{
    calls.remove(id);
}

LocalSegment::LocalSegment(int cid, const Call *cp, const Endpoint *ep) :
Segment(cid, cp, ep->sip()), endpoint(ep)
{
}

RemoteSegment::RemoteSegment(int cid, const Call *cp, const Provider *pp) :
Segment(cid, cp, pp->sip()), provider(pp)
{
}
