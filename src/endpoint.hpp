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
#include "address.hpp"
#include "context.hpp"
#include <QElapsedTimer>

class Registry;

class Endpoint final
{
    Q_DISABLE_COPY(Endpoint)

public:
    Endpoint(Context *ctx, Address addr, int expires, Registry *reg = nullptr);
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
    
    int expires(void) const;
    void refresh(int expires);
    
    static Endpoint *find(Context *ctx, const Address& addr);

private:
    Registry *registry;         // registry that holds our endpoint
    Context *context;           // context endpoint exists on
    Address address;            // network address of endpoint
    QElapsedTimer updated;      // last refreshed registration
    qint64 expiration;          // msecs to expiration
};

QDebug operator<<(QDebug dbg, const Endpoint& endpoint);
