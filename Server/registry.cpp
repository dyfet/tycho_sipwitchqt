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

static QHash<UString, QCryptographicHash::Algorithm> digests = {
    {"MD5",     QCryptographicHash::Md5},
    {"SHA",     QCryptographicHash::Sha1},
    {"SHA1",    QCryptographicHash::Sha1},
    {"SHA2",    QCryptographicHash::Sha256},
    {"SHA256",  QCryptographicHash::Sha256},
    {"SHA512",  QCryptographicHash::Sha512},
    {"SHA-1",   QCryptographicHash::Sha1},
    {"SHA-256", QCryptographicHash::Sha256},
    {"SHA-512", QCryptographicHash::Sha512},
};

// We create registration records based on the initial pre-authorize
// request, and as inactive.  The registration becomes active only when
// it is updated by an authorized request.
Registry::Registry(const QVariantHash &ep) :
expires(-1), context(nullptr), endpoint(ep)
{    
    text = ep.value("display").toString();
    alias = ep.value("name").toString();
    number = ep.value("number").toInt();
    label = ep.value("label").toString();
    expires = ep.value("expires").toInt() * 1000l;

    updated.start();

    QPair<int,UString> key(number, label);
    extensions.insert(number, this);
    aliases.insert(alias, this);
    registries.insert(key, this);
    qDebug() << "Initializing" << key;
}

Registry::~Registry()
{
    if(!context)
        qDebug() << "Abandoning" << number << label;
    else {
        qDebug() << "Releasing" << number << label;
        // may later kill active calls, etc...
    }
    QPair<int,UString> key(number, label);
    registries.remove(key);
    extensions.remove(number, this);
    aliases.remove(alias, this);
}

QList<Registry *> Registry::list()
{
    return extensions.values();
}

// to find a registration record associated with a registration event
Registry *Registry::find(const Event& event)
{
    QPair<int,UString> key(event.number(), event.label());
    auto *reg = registries.value(key, nullptr);
    qDebug() << "FINDING" << key << reg;
    if(reg && reg->hasExpired()) {
        delete reg;
        return nullptr;
    }
    return reg;
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
int Registry::authorize(const Event& ev)
{
    UString algo = endpoint.value("digest").toString();
    UString secret = endpoint.value("secret").toString();
    UString nonce = random.toHex().toLower();
    UString method = ev.method();
    UString uri = ev.request();

    if(ev.authorizingRealm() != endpoint.value("realm").toString())
        return SIP_FORBIDDEN;

    if(ev.authorizingId() != endpoint.value("user").toString())
        return SIP_FORBIDDEN;

    if(ev.authorizingAlgorithm() != algo)
        return SIP_FORBIDDEN;

    if(ev.authorizingOnce() != nonce)
        return SIP_FORBIDDEN;

    auto digest = digests[algo];
    UString ha2 = QCryptographicHash::hash(method + ":" + uri, digest).toHex().toLower();
    UString expected = QCryptographicHash::hash(secret + ":" + nonce + ":" + ha2, digest).toHex().toLower();

    if(expected != ev.authorizingDigest())
        return SIP_FORBIDDEN;

    // de-registration
    if(ev.expires() < 1) {
        expires = 0;
        qDebug() << "De-registering" << ev.number() << ev.label();
        return SIP_OK;
    }

    if(!context)
        qDebug() << "Registering" << ev.number() << ev.label() << "for" << endpoint.value("expires");
    else
        qDebug() << "Refreshing" << ev.number() << ev.label() << "for" << endpoint.value("expires");

    context = ev.context();
    address = ev.contact();
    updated.restart();
    return SIP_OK;
}

QDebug operator<<(QDebug dbg, const Registry& registry)
{
    
    dbg.nospace() << "Registry(" << registry.data() << ")";
    return dbg.maybeSpace();
}
