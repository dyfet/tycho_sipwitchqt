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

#include "manager.hpp"

#include <QMultiHash>

static QMultiHash<int, Registry*> extensions;
static QMultiHash<UString, Registry*> aliases;
static QHash<QPair<int,UString>, Registry *> registries;

Registry::Registry(const QSqlRecord& db, const Event &event) :
extension(db)
{    
    text = db.value("display").toString();
    alias = db.value("name").toString();
    number = db.value("number").toInt();
    label = db.value("label").toString();
    agent = event.agent();

    QPair<int,UString> key(number, label);
    extensions.insert(number, this);
    aliases.insert(alias, this);
    registries.insert(key, this);
}

Registry::~Registry()
{
    QPair<int,UString> key(number, label);
    registries.remove(key);
    extensions.remove(number, this);
    aliases.remove(alias, this);
}

QList<Registry *> Registry::list()
{
    return extensions.values();
}

Registry *Registry::find(const QSqlRecord& db)
{
    QPair<int,UString> key(db.value("number").toInt(), db.value("label").toString());
    return registries.value(key, nullptr);
}

QList<Registry *> Registry::find(const UString& target)
{
    QList<Registry *> list;

    if(target.length() < 1)
        return list;

    if(target.toInt() > 0) {
        list = extensions.values(target.toInt());
        if(list.count() > 0)
            return list;
    }

    return aliases.values(target);
}

// event handling for registration system as a whole...
void Registry::process(const Event& ev)
{
    Q_UNUSED(ev);
}

// authorize registration processing
void Registry::authorize(const Event& ev)
{
    Q_UNUSED(ev);
}

QDebug operator<<(QDebug dbg, const Registry& registry)
{
    
    dbg.nospace() << "Registry(" << registry.data() << ")";
    return dbg.maybeSpace();
}
