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

#include "compiler.hpp"
#include "provider.hpp"

class Call;

// basic call processing segment (call leg), can be for remote or local...
class Segment
{
    Q_DISABLE_COPY(Segment)

protected:
    const Call *call;
    const Context *context;
    int id;

    Segment(int cid, const Call *cp, const Context *ctx);
    virtual ~Segment();
};

class LocalSegment final : public Segment 
{
    Q_DISABLE_COPY(LocalSegment)

public:
    LocalSegment(int cid, const Call *cp, const Endpoint *ep);

private:
    const Endpoint *endpoint;
};

class RemoteSegment final : public Segment 
{
    Q_DISABLE_COPY(RemoteSegment)

public:
    RemoteSegment(int cid, const Call *cp, const Provider *prov);

private:
    const Provider *provider;
};


QDebug operator<<(QDebug dbg, const Segment& seg);
