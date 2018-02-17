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
static unsigned count[1000];
static unsigned char online[1000 / 8];
static bool init = false;
static QPair<int,int> range;
static int size = 0;

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

static void set(int number)
{
    number -= range.first;
    auto offset = number / 8;
    auto mask = static_cast<unsigned char>(1 << (number % 8));
    online[offset] |= mask;
}

static void unset(int number)
{
    number -= range.first;
    auto offset = number / 8;
    auto mask = static_cast<unsigned char>(1 << (number % 8));
    online[offset] &= ~mask;
}

// We create registration records based on the initial pre-authorize
// request, and as inactive.  The registration becomes active only when
// it is updated by an authorized request.
Registry::Registry(const QVariantHash &ep) :
timeout(-1), context(nullptr)
{    
    if(!init) {
        range = Database::range();
        size = ((range.second - range.first) / 8) + 1;
        memset(count, 0, sizeof(count));
        init = true;
    }

    userDisplay = ep.value("display").toString().toUtf8();
    userId = ep.value("user").toString();
    userSecret = ep.value("secret").toString().toUtf8();
    number = ep.value("number").toInt();
    userLabel = ep.value("label").toString().toUtf8();
    timeout = ep.value("expires").toInt() * 1000l;
    userMembership = ep.value("groups").toByteArray();
    authRealm = ep.value("realm").toString().toUtf8();
    authDigest = ep.value("digest").toString().toUpper();

    updated.start();

    if(++count[number] == 1)
        set(number);

    QPair<int,UString> key(number, userLabel);
    extensions.insert(number, this);
    aliases.insert(userId, this);
    registries.insert(key, this);
    qDebug() << "Initializing" << key;
}

Registry::~Registry()
{
    if(!context)
        qDebug() << "Abandoning" << number << userLabel;
    else {
        qDebug() << "Releasing" << number << userLabel;
        // may later kill active calls, etc...
    }

    if(--count[number] == 0) {
        unset(number);
    }

    QPair<int,UString> key(number, userLabel);
    registries.remove(key);
    extensions.remove(number, this);
    aliases.remove(userId, this);
}

void Registry::cleanup(void)
{
    auto snapshot = registries;
    foreach(auto reg, snapshot) {
        if(reg->hasExpired())
            delete reg;
    }
}

UString Registry::bits()
{
    if(size < 1)
        return UString();

    QByteArray result(reinterpret_cast<const char *>(&online), size);
    return result.toBase64();
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

UString Registry::activity() const
{
    unsigned char bitmap[sizeof(online)];
    memcpy(bitmap, online, sizeof(bitmap));

    if(size < 1)
        return UString();

    auto cp = &bitmap[0];
    auto mp = reinterpret_cast<const unsigned char *>(userMembership.constData());
    auto count = userMembership.size();
    while(mp && count--) {
        *(cp++) |= (*mp++);
    }

    if(size < 1)
        return UString();

    QByteArray result(reinterpret_cast<const char *>(&bitmap), size);
    return result.toBase64();
}

// authorize registration processing
int Registry::authorize(const Event& ev)
{
    UString nonce = random.toHex().toLower();
    UString method = ev.method();
    UString uri = ev.request();

    if(ev.authorizingRealm() != authRealm)
        return SIP_FORBIDDEN;

    if(ev.authorizingId() != userId)
        return SIP_FORBIDDEN;

    if(ev.authorizingAlgorithm() != authDigest)
        return SIP_FORBIDDEN;

    if(ev.authorizingOnce() != nonce)
        return SIP_FORBIDDEN;

    auto digest = digests[authDigest];
    UString ha2 = QCryptographicHash::hash(method + ":" + uri, digest).toHex().toLower();
    UString expected = QCryptographicHash::hash(userSecret + ":" + nonce + ":" + ha2, digest).toHex().toLower();

    if(expected != ev.authorizingDigest())
        return SIP_FORBIDDEN;

    // de-registration
    if(ev.expires() < 1) {
        timeout = 0;
        qDebug() << "De-registering" << ev.number() << ev.label();
        return SIP_OK;
    }

    qDebug() << "REGISTERING WITH " << ev.did() << ev.cid() << ev.tid();

    if(!context)
        qDebug() << "Registering" << ev.number() << ev.label() << "for" << timeout / 1000l;
    else
        qDebug() << "Refreshing" << ev.number() << ev.label() << "for" << timeout / 1000l;

    auto protocol = ev.protocol();
    if(protocol == "udp")
        address = ev.source();  // for nat, use appearing origin...
    else
        address = ev.contact();
    context = ev.context();
    updated.restart();

    //some testing for core message code...
    //context->message("system", UString::number(number), address.toString(), {{"Subject", "Hello World"}});

    return SIP_OK;
}

QDebug operator<<(QDebug dbg, const Registry& registry)
{
    
    dbg.nospace() << "Registry(" << registry.user() << registry.expires() << ")";
    return dbg.maybeSpace();
}
