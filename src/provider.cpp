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

#include "provider.hpp"
#include <QHash>
#include <QVariant>
#include <QDebug>

static QHash<const QString, Provider *> providers;
static QHash<int, Provider *> registrations;

Provider::Provider(const QSqlRecord& db, Context *ctx, int rid) :
provider(db), uri(db.value("contact").toString()), text(db.value("display").toString()), context(ctx), id(rid)
{    
    providers.insert(uri, this);

    if(id > -1)
        registrations.insert(id, this);
}

Provider::~Provider()
{
    providers.remove(uri);

    if(id > -1)
        registrations.remove(id);
}

Provider *Provider::find(int rid)
{
    return registrations.value(rid);
}

Provider *Provider::find(const QString& target)
{
    return providers.value(target);
}

QList<Provider *> Provider::list()
{
    return providers.values();
}

void Provider::reload(const QList<QSqlRecord>& records)
{
    foreach(auto provider, providers.values()) {
        delete provider;
    }
    foreach(auto record, records) {

    }
}

QDebug operator<<(QDebug dbg, const Provider& prov)
{
    
    dbg.nospace() << "Provider(" << prov.data() << ")";
    return dbg.maybeSpace();
}
