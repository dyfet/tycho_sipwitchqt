/*
 * Copyright (C) 2017-2018 Tycho Softworks.
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
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#include "../Common/compiler.hpp"
#include "crypto.hpp"

#include <QCryptographicHash>

#ifdef Q_OS_MAC
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonRandom.h>
#else
#include <openssl/rand.h>
#endif

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/hmac.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

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

QPair<QByteArray,QByteArray> Crypto::keypair(bool compressed)
{
    auto pair = EC_KEY_new_by_curve_name(NID_secp256k1);
    if(!pair)
        return {};

    quint8 buffer[32];
    random(buffer, sizeof(buffer));
    auto privKey = BN_bin2bn(buffer, sizeof(buffer), nullptr);
    EC_KEY_set_private_key(pair, privKey);
    auto ctx = BN_CTX_new();
    BN_CTX_start(ctx);
    auto grp = EC_KEY_get0_group(pair);
    auto pubKey = EC_POINT_new(grp);
    EC_POINT_mul(grp, pubKey, privKey, nullptr, nullptr, ctx);
    EC_KEY_set_public_key(pair, pubKey);

    EC_POINT_free(pubKey);
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
    BN_clear_free(privKey);

    QByteArray priv, pub;
    char pubOut[65], privOut[320];
    auto pubPtr = reinterpret_cast<unsigned char *>(pubOut);
    auto privPtr = reinterpret_cast<unsigned char *>(privOut);
    auto bufPtr = reinterpret_cast<char *>(buffer);

    if(compressed)
        EC_KEY_set_conv_form(pair, POINT_CONVERSION_COMPRESSED);
    else {
        EC_KEY_set_conv_form(pair, POINT_CONVERSION_UNCOMPRESSED);
        EC_KEY_set_asn1_flag(pair, OPENSSL_EC_NAMED_CURVE);
    }

    int privLen = 32;
    int pubLen = i2o_ECPublicKey(pair, nullptr);
    Q_ASSERT(pubLen <= static_cast<int>(sizeof(pubOut)));
    i2o_ECPublicKey(pair, &pubPtr);

    if(!compressed) {
        bufPtr = privOut;
        privLen = i2d_ECPrivateKey(pair, nullptr);
        Q_ASSERT(privLen <= static_cast<int>(sizeof(privOut)));
        i2d_ECPrivateKey(pair, &privPtr);
    }

    EC_KEY_free(pair);
    return {QByteArray(pubOut, pubLen), QByteArray(bufPtr, privLen)};
}



