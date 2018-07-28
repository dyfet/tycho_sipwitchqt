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
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/rand.h>
#endif

static quint8 *bytePointer(const char *cp)
{
    return reinterpret_cast<quint8*>(const_cast<char *>(cp));
}

Crypto::Data::Data()
{
    auto context = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    EVP_PKEY *params = nullptr;
    pkey = nullptr;

    if(!context)
        return;

    if(EVP_PKEY_paramgen_init(context) || EVP_PKEY_CTX_set_ec_paramgen_curve_nid(context, NID_secp256k1) != 1 || !EVP_PKEY_paramgen(context, &params)) {
        EVP_PKEY_CTX_free(context);
        return;
    }

    auto keygen = EVP_PKEY_CTX_new(params, nullptr);
    if(!keygen) {
        EVP_PKEY_CTX_free(context);
        return;
    }

    if(EVP_PKEY_keygen_init(keygen) != 1 || EVP_PKEY_keygen(keygen, &pkey) != 1) {
        EVP_PKEY_CTX_free(context);
        EVP_PKEY_CTX_free(keygen);
        return;
    }

    EVP_PKEY_CTX_free(context);
    EVP_PKEY_CTX_free(keygen);

    BIO *bp = BIO_new(BIO_s_mem());
    if(!bp) {
        return;
    }

    if(PEM_write_bio_PUBKEY(bp, pkey) == 1) {
        BUF_MEM *buf;
        BIO_get_mem_ptr(bp, &buf);
        pem = QByteArray(buf->data, static_cast<int>(buf->length));
    }
    BIO_free(bp);
}

Crypto::Data::~Data()
{
    if(pkey) {
        EVP_PKEY_free(pkey);
        pkey = nullptr;
    }
}

Crypto::Crypto()
{
    d = new Crypto::Data();
}

QByteArray Crypto::sharedKey(const QByteArray& peer) const
{
    BUF_MEM *buf = BUF_MEM_new();
    BUF_MEM_grow(buf, static_cast<size_t>(peer.count()));
    BIO *bp = BIO_new(BIO_s_mem());
    memcpy(buf->data, peer.constData(), static_cast<size_t>(peer.count()));
    BIO_set_mem_buf(bp, buf, BIO_NOCLOSE);
    auto key = PEM_read_bio_PUBKEY(bp, nullptr, nullptr, nullptr);
    BIO_free(bp);
    BUF_MEM_free(buf);
    size_t len = 0;
    auto ctx = EVP_PKEY_CTX_new(d->pkey, nullptr);
    if(!ctx) {
        EVP_PKEY_free(key);
        return {};
    }
    if(EVP_PKEY_derive_init(ctx) != 1 || EVP_PKEY_derive_set_peer(ctx, key) != 1 || EVP_PKEY_derive(ctx, nullptr, &len) != 1 || len < 1) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(key);
        return {};
    }
    QByteArray result(static_cast<int>(len), 0);
    auto cp = bytePointer(result.constData());
    EVP_PKEY_derive(ctx, cp, &len);
    return result;
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

QByteArray Crypto::pad(const QByteArray& unpadded, int size)
{
    Q_ASSERT(size > 0 && size <= 64);

    auto data = unpadded;
    auto count = data.count();
    auto offset = count % size;

    if(!offset) {
        data += random(size - 1) + static_cast<char>(size);
    }
    else {
        size -= offset;
        if(size > 1)
            data += random(size - 1);
        data += static_cast<char>(size);
    }
    return data;
}

QByteArray Crypto::unpad(const QByteArray& padded, int iv)
{
    Q_ASSERT(padded.count() > iv);

    QByteArray data;
    if(iv)
        data = padded.mid(iv);
    else
        data = padded;

    auto count = data.count();
    auto cp = data.constData() + count - 1;
    data.resize(count - *cp);
    return data;
}

QByteArray Crypto::encrypt(const QByteArray& data, const QByteArray& key)
{
    const EVP_CIPHER *algo;
    switch(key.count()) {
    case 16:
        algo = EVP_aes_128_ecb();
        break;
    case 32:
        algo = EVP_aes_256_ecb();
        break;
    default:
        return {};
    }

    auto context = EVP_CIPHER_CTX_new();
    if(!context)
        return {};

    auto align = EVP_CIPHER_block_size(algo);
    auto ivbuf = random(align);
    auto iv = bytePointer(ivbuf.constData());
    auto kp = reinterpret_cast<const quint8*>(key.constData());

    EVP_CIPHER_CTX_init(context);
    if(EVP_DecryptInit_ex(context, algo, nullptr, kp, iv) != 1) {
        EVP_CIPHER_CTX_free(context);
        return {};
    }

    EVP_CIPHER_CTX_set_padding(context, 0);
    auto in = Crypto::pad(data, align);
    in += sha256(in);
    auto size = 0;

    QByteArray out(in.count() + align, 0);
    auto ip = reinterpret_cast<const quint8 *>(in.constData());
    auto op = bytePointer(out.constData());
    if(EVP_EncryptUpdate(context, op, &size, ip, in.count()))
        out.resize(size);
    else
        out = QByteArray{};
    EVP_CIPHER_CTX_cleanup(context);
    EVP_CIPHER_CTX_free(context);
    return out;
}

QByteArray Crypto::decrypt(const QByteArray& data, const QByteArray& key)
{
    const EVP_CIPHER *algo;
    switch(key.count()) {
    case 16:
        algo = EVP_aes_128_ecb();
        break;
    case 32:
        algo = EVP_aes_256_ecb();
        break;
    default:
        return {};
    }

    auto context = EVP_CIPHER_CTX_new();
    if(!context)
        return {};

    auto align = EVP_CIPHER_block_size(algo);
    auto ivbuf = QByteArray(align, 0);
    auto iv = bytePointer(ivbuf.constData());
    auto kp = reinterpret_cast<const quint8*>(key.constData());

    EVP_CIPHER_CTX_init(context);
    if(EVP_DecryptInit_ex(context, algo, nullptr, kp, iv) != 1) {
        EVP_CIPHER_CTX_free(context);
        return {};
    }

    EVP_CIPHER_CTX_set_padding(context, 0);
    auto size = 0;

    QByteArray out(data.count(), 0);
    auto ip = reinterpret_cast<const quint8 *>(data.constData());
    auto op = bytePointer(out.constData());
    if(EVP_DecryptUpdate(context, op, &size, ip, data.count())) {
        auto sha = out.right(32);   // extract sha
        out.resize(size - 32);      // strip sha
        if(sha == sha256(out))
            out = Crypto::unpad(out);
        else
            out = QByteArray{};
    }
    else
        out = QByteArray{};
    EVP_CIPHER_CTX_cleanup(context);
    EVP_CIPHER_CTX_free(context);
    return out;
}

QByteArray Crypto::keygen(const QByteArray& secret, int keysize, int rounds, const QByteArray& salt)
{
    switch(keysize) {
    case 16:
    case 128:
        keysize = 16;
        break;
    case 32:
    case 256:
        keysize = 32;
        break;
    default:
        return {};
    }
    const EVP_MD *digest = EVP_sha256();
    QByteArray out(keysize, 0);
    auto op = bytePointer(out.constData());
    auto sp = reinterpret_cast<const quint8*>(salt.constData());
    if(salt.count() < 1)
        sp = nullptr;

    if(PKCS5_PBKDF2_HMAC(secret.constData(), secret.count(), sp, salt.count(), rounds, digest, keysize, op) != 1)
        return {};

    return out;
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

QByteArray Crypto::sha512(const QByteArray& data)
{
    auto dp = reinterpret_cast<const quint8 *>(data.constData());
    auto size = static_cast<size_t>(data.count());
    quint8 digest[128];
#ifdef Q_OS_MAC
    CC_SHA512_CTX context;
    CC_SHA512_Init(&context);
    CC_SHA512_Update(&context, dp, static_cast<CC_LONG>(size));
    CC_SHA512_Final(digest, &context);
#else
    SHA512_CTX context;
    SHA512_Init(&context);
    SHA512_Update(&context, dp, size);
    SHA512_Final(digest, &context);
#endif
    return {reinterpret_cast<char *>(digest), static_cast<int>(size)};
}

QByteArray Crypto::sha256(const QByteArray& data)
{
    auto dp = reinterpret_cast<const quint8 *>(data.constData());
    auto size = static_cast<size_t>(data.count());
    quint8 digest[64];
#ifdef Q_OS_MAC
    CC_SHA256_CTX context;
    CC_SHA256_Init(&context);
    CC_SHA256_Update(&context, dp, static_cast<CC_LONG>(size));
    CC_SHA256_Final(digest, &context);
#else
    SHA256_CTX context;
    SHA256_Init(&context);
    SHA256_Update(&context, dp, size);
    SHA256_Final(digest, &context);
#endif
    return {reinterpret_cast<char *>(digest), static_cast<int>(size)};
}

bool Crypto::verify(const QByteArray& pubkey, const QByteArray& data, const QByteArray& sig)
{
    EC_KEY *pair = nullptr;
    if(pubkey.count() == 33) {
        pair = EC_KEY_new_by_curve_name(NID_secp256k1);
        auto kp = reinterpret_cast<const quint8 *>(pubkey.constData());
        o2i_ECPublicKey(&pair, &kp, 33);
    }

    if(!pair)
        return false;

    auto sp = reinterpret_cast<const quint8 *>(sig.constData());
    auto signature = d2i_ECDSA_SIG(nullptr, &sp, sig.count());
    auto digest = sha256(data);
    auto verified = false;
    if(ECDSA_do_verify(reinterpret_cast<const quint8*>(digest.constData()), digest.count(), signature, pair) == 1)
        verified = true;
    ECDSA_SIG_free(signature);
    EC_KEY_free(pair);
    return verified;
}

QByteArray Crypto::sign(const QByteArray& prikey, const QByteArray& data)
{
    EC_KEY *pair = nullptr;
    if(prikey.count() == 32) {
        pair = EC_KEY_new_by_curve_name(NID_secp256k1);
        auto privKey = BN_bin2bn(reinterpret_cast<const quint8*>(prikey.constData()),prikey.count(), nullptr);
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
    }

    if(!pair)
        return {};

    auto digest = sha256(data);
    auto signature = ECDSA_do_sign(reinterpret_cast<const quint8*>(digest.constData()), digest.count(), pair);
    char result[128];
    auto size = ECDSA_size(pair);
    auto dp = reinterpret_cast<quint8 *>(result);
    Q_ASSERT(size <= 128);
    i2d_ECDSA_SIG(signature, &dp);
    ECDSA_SIG_free(signature);
    EC_KEY_free(pair);
    return {result, size};
}
