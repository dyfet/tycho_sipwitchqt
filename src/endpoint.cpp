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

#include "endpoint.hpp"
#include <QElapsedTimer>

static QHash<const Context *, QHash<Address, Endpoint *>> Endpoints;

Endpoint::Endpoint(Context *ctx, const Address& addr, int expires, Registry *reg) :
registry(reg), context(ctx), address(addr)
{
    refresh(expires);
    Endpoints[ctx].insert(address, this);
}

Endpoint::~Endpoint()
{
    Endpoints[context].remove(address);
}

Endpoint *Endpoint::find(const Context *ctx, const Address& addr) {
    if(Endpoints.contains(ctx))
        return Endpoints[ctx].value(addr, nullptr);
    else
        return nullptr;
}

void Endpoint::refresh(int expires)
{
    updated.start();
    expiration = expires * 1000l;
}  

int Endpoint::expires() const
{
    auto elapsed = updated.elapsed();
    if(elapsed >= expiration)
        return 0;
    return (int)((expiration - elapsed) / 1000l);
}

QDebug operator<<(QDebug dbg, const Endpoint& endpoint)
{
    dbg.nospace() << "Endpoint(" << endpoint.host() << ":" << endpoint.port() << ")";
    return dbg.maybeSpace();
}
