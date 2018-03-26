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

#include "crypto.hpp"

#include <QCryptographicHash>

#ifdef Q_OS_MAC
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonRandom.h>


#else
#include <openssl/rand.h>
#endif

static quint8 *bytePointer(const char *cp)
{
    return reinterpret_cast<quint8*>(const_cast<char *>(cp));
}

const QByteArray Crypto::random(int size)
{
    QByteArray result(size, 0);
    auto cp = bytePointer(result.constData());

#ifdef Q_OS_MAC
    if(CCRandomGenerateBytes(cp, static_cast<size_t>(size)) != kCCSuccess)
        return QByteArray();
#else
    if(!RAND_bytes(cp, size))
        return QByteArray();
#endif
    return result;
}

bool Crypto::random(quint8 *cp, int size)
{
#ifdef Q_OS_MAC
    if(CCRandomGenerateBytes(cp, static_cast<size_t>(size)) != kCCSuccess)
        return false;
#else
    if(!RAND_bytes(cp, size))
        return false;
#endif
    return true;
}



