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

/**
  * These classes are used by the stack to keep track of active call sessions.
  * Call sessions are handled by a master object which has a call segment for
  * each b2bua endpoint.  Because call segments are handled differently for
  * locally managed devices and remote (provider or p2p) connections, the
  * segment is split into a base and derived local and remote classes.
  * @file call.hpp
  */

#include "compiler.hpp"
#include "provider.hpp"

class Call;
class RemoteSegment;

// basic call processing segment (call leg), can be for remote or local...
class Segment
{
    friend class Call;
    Q_DISABLE_COPY(Segment)

public:
    static QList<RemoteSegment *>peers();

protected:
    Call *call;
    Context *context;
    int id;

    Segment(int cid, Call *cp, Context *ctx);
    virtual ~Segment();
};

class LocalSegment final : public Segment 
{
    Q_DISABLE_COPY(LocalSegment)

public:
    LocalSegment(int cid, Call *cp, Endpoint *ep);

private:
    const Endpoint *endpoint;
    const Registry *registry;
};

class RemoteSegment final : public Segment 
{
    Q_DISABLE_COPY(RemoteSegment)

public:
    RemoteSegment(int cid, Call *cp, Provider *prov);   // standard provider
    RemoteSegment(int cid, Call *cp, Context *ctx);     // p2p calls...
    ~RemoteSegment();

private:
    const Provider *provider;
};

class Call
{
    Q_DISABLE_COPY(Call)

public:
    Call(const QString id);
    ~Call();

    inline Segment *origin() const {
        return from;
    }

    inline const QString display() const {
        return text;
    }

    static QList<Call*> list();

private:
    Segment *from;
    QList<Segment*> to;
    QString tag;
    QString text;
};

QDebug operator<<(QDebug dbg, const Segment& seg);
QDebug operator<<(QDebug dbg, const Call& cr);
