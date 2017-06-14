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

#include "registry.hpp"

static QHash <const QString, Registry *> extensions, aliases;

Registry::Registry(const QSqlRecord& db) :
extension(db), id(db.value("number").toString()), alias(db.value("alias").toString())
{    
    if(id.toInt() > 0)
        extensions.insert(id, this);

    if(alias.length() > 0)
        aliases.insert(alias, this);
}

Registry::~Registry()
{
    if(id.toInt() > 0)
        extensions.remove(id);

    if(alias.length() > 0)
        aliases.remove(alias);

    foreach(auto endpoint, endpoints) {
        delete endpoint;
    }
}

QList<Registry *> Registry::list()
{
    return extensions.values();
}

// find at least one un-expired endpoint...
bool Registry::hasExpired() const
{
    foreach(auto endpoint, endpoints) {
        if(!endpoint->hasExpired())
            return false;
    }
    return true;
}

// find longest remaining time in endpoints...
int Registry::expires() const
{
    int remaining = 0;
    foreach(auto endpoint, endpoints) {
        int expiration = endpoint->expires();
        if(expiration > remaining)
            remaining = expiration;
    }
    return remaining;
}

Registry *Registry::find(const QString& target)
{
    Registry *registry = nullptr;

    if(target.length() < 1)
        return nullptr;

    if(target.toInt() > 0)
        registry = extensions.value(target, nullptr);

    if(!registry)
        registry = aliases.value(target, nullptr);

    return registry;
}

QDebug operator<<(QDebug dbg, const Registry& registry)
{
    
    dbg.nospace() << "Registry(" << registry.data() << ")";
    return dbg.maybeSpace();
}
