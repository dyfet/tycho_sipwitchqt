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

Provider::Provider(const QSqlRecord& db) :
provider(db), uri(db.value("contact").toString())
{    
    providers.insert(uri, this);
}

Provider::~Provider()
{
    providers.remove(uri);
}

Provider *Provider::find(const QString& target)
{
    return providers.value(target);
}

QDebug operator<<(QDebug dbg, const Provider& prov)
{
    
    dbg.nospace() << "Provider(" << prov.data() << ")";
    return dbg.maybeSpace();
}
