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

#ifndef CRYPTO_HPP_
#define CRYPTO_HPP_

#include "compiler.hpp"
#include "types.hpp"

#include <QCoreApplication>
#include <QString>
#include <QByteArray>
#include <QSharedData>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <QCryptographicHash>

class Crypto final
{
public:
    Crypto();
    Crypto(const Crypto& copy) = default;
    ~Crypto() = default;

    explicit operator bool() const {
        return !d->pem.isEmpty();
    }

    bool operator!() const {
        return d->pem.isEmpty();
    }

    QByteArray publicKey() const {
        return d->pem;
    }

    QByteArray sharedKey(const QByteArray& peer) const;

    static QHash<UString, QCryptographicHash::Algorithm> digests;
    static const QByteArray random(int size);
    static bool random(quint8 *bytes, int size);
    static QPair<QByteArray, QByteArray> keypair(bool compressed = true);
    static QByteArray pad(const QByteArray &unpadded, int size);
    static QByteArray unpad(const QByteArray& data, int iv = 0);
    static QByteArray encrypt(const QByteArray& data, const QByteArray& key, bool isSigned = false);
    static QByteArray decrypt(const QByteArray& data, const QByteArray& key, bool isSigned = false);
    static QByteArray encrypt(const QByteArray& data, const QByteArray& key, const QByteArray& pri);
    static QByteArray decrypt(const QByteArray& data, const QByteArray& key, const QByteArray& pub);
    static QByteArray sha256(const QByteArray& data);
    static QByteArray sha512(const QByteArray& data);
    static QByteArray sign(const QByteArray& prikey, const QByteArray& data);
    static QByteArray keygen(const QByteArray& secret, int keysize, int rounds = 1000, const QByteArray& salt = {});
    static bool verify(const QByteArray& pubkey, const QByteArray& data, const QByteArray& sig);

private:
    class Data final : public QSharedData
    {
        Q_DISABLE_COPY(Data)
    public:
        Data();
        ~Data();

        EVP_PKEY *pkey;
        QByteArray pem;     // public key to send to peer...
    };

    QSharedDataPointer<Crypto::Data> d;
};

Q_DECLARE_METATYPE(Crypto)

/*!
 * Some cryptographic related utility functions and key pair class.  This
 * provides interfaces to lower level system crypto functions.
 * \file crypto.hpp
 */

/*!
 * \class Crypto
 * \brief Cryptographic functions and key pair processing.
 * This is used to provide both low level crypto functions and to offer key
 * management for ephemeral sessions, such as for establishing a dh key
 * exchange of a shared secret between active endpoints.
 * \author David Sugar <tychosoft@gmail.com>
 *
 * \fn Crypto::Crypto()
 * Constructs an ephemeral key object for use in session based shared secret
 * exchanges.
 *
 * \fn Crypto::publicKey()
 * Provides the public key for this session to share with a remote peer.
 * \return byte array of pem encoded public key.
 *
 * \fn Crypto::sharedKey(const QByteArray& peer)
 * Computes a derived shared secret using the remote nodes public key.  For
 * this to really be secure it is recommended that the public keys are signed
 * with a trusted key.  It is also recommended that the shared secret be
 * salted and passed thru a digest algorithm if inteded to be used as a AES
 * key.
 * \param peer PEM encoded public key of remote peer instance of Crypto.
 * \return shared secret derived.
 *
 * \fn Crypto::random(int size)
 * Produce a random binary array using system random number generator.
 * \param size Number of random bytes requested.
 * \return a byte array of random values if successful.
 *
 * \fn Crypto::random(quint8 *bytes, int size)
 * Fills a section of memory with random binary data from the system random
 * number generator.
 * \param bytes Memory location to fill.
 * \param size Number of random bytes requested.
 * \return true if successful.
 *
 * \fn Crypto::pad(const QByteArray& data, int size)
 * Padds a data block to align data for a cipher.
 * \param data Binary data to pad
 * \param size Alignment size of padding for cipher.
 * \return padded byte array object.
 *
 * \fn Crypto::unpad(const QByteArray& data, int iv)
 * Remove padding that was added for cipher.  This also can be used to
 * remove a pre-pended iv.
 * \param data Binary data that was padded.
 * \param iv Pre-pended iv size.
 * \return unpadded byte array object.
 *
 * \fn Crypto::keygen(const QByteArray& secret, int keysize, int rounds, const QByteArray& salt)
 * Generate a symetric cipher key from a passphrase or shared secret. This is used to
 * produce a clean distribution of supplied secret.
 * \param secret A pass phrase or shared secret to start from.
 * \param keysize For AES 16 or 32.
 * \param rounds Number of iterations for key generation.
 * \param salt Optional salt to apply.
 * \return a derived AES crypto key.
 *
 * \fn Crypto::encrypt(const QByteArray& data, const QByteArray& key, bool isSigned)
 * This is used to produce an encrypted output of supplied data using AES.
 * The iv should be appended as random data.  The data block is aligned to
 * the cipher block size (32 or 64 for aes 128 or 256), and a sha256 can be
 * appended and ciphered at the end so the decryptor can really verify it
 * has valid data rather than just purely random values.
 * \param data What to encrypt.
 * \param key Symetric key to encrypt with.
 * \param isSigned Skip digest if signed.
 * \return ciphered data block, ciphered iv in front, padded, tail sha256.
 *
 * \fn Crypto::decrypt(const QByteArray& data, const QByteArray& key, bool isSigned)
 * This used to decrypt a data bock produced by encrypt.  It can also verify
 * the sha256 at the end.
 * \param data What to decrypt.
 * \param key Symetric key to decrypt with.
 * \param isSiged Skip digest if signed.
 * \return clear text of ciphered data.
 *
 * \fn Crypto::encrypt(const QByteArray& data, const QByteArray& key, const QByteArray& pri)
 * This is used to produce an encrypted output of supplied data using AES.
 * The iv should be appended as random data.  The data block is aligned to
 * the cipher block size (32 or 64 for aes 128 or 256), and a signature is
 * appended and ciphered at the end so the decryptor can really verify it
 * has valid data.
 * \param data What to encrypt.
 * \param key Symetric key to encrypt with.
 * \param pri Private key to sign block before encrypting.
 * \return ciphered data block, ciphered iv in front, padded, tail signature.
 *
 * \fn Crypto::decrypt(const QByteArray& data, const QByteArray& key, const QByteArray& pub)
 * This used to decrypt a data bock produced by encrypt.  It also verifies the
 * signature with the matching public key at the end.
 * \param data What to decrypt.
 * \param key Symetric key to decrypt with.
 * \param pri Key to verify signature.
 * \return clear text of ciphered data.
 *
 * \fn Crypto::sign(const QByteArray& key, const QByteArray& data)
 * Sign a data block using the private key.  This presumes a compressed ec
 * private key and the secp256k1 curve is being used.
 * \param key Private key (32 byte original random compressed values).
 * \param data Binary data to be signed.
 * \return generated signature that can be verified.
 *
 * \fn Crypto::verify(const QByteArray& key, const QByteArray& data, const QByteArray& sig)
 * Verify a private key signed data block using the published public key.  This
 * presumes a compressed ec publoc key and the secp256k1 curve is being used.
 * \param key Public key (33 bytes compressed public key).
 * \param data Binary data to verify.
 * \param sig Signature generated from private key.
 * \return true if signature matches data.
 */

#endif
