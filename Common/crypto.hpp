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

namespace Crypto {
    const QByteArray random(int size);
    bool random(quint8 *bytes, int size);
    QPair<QByteArray, QByteArray> keypair(bool compressed = true);
    void pad(QByteArray &data, int size);
    void unpad(QByteArray& data, int iv = 0);
}

/*!
 * Some cryptographic related utility functions.  This provides
 * interfaces to lower level system crypto functions.
 * \file crypto.hpp
 */

/*!
 * \namespace Crypto
 * \brief Cryptographic functions.
 * This namespace is typically used to access system cryptographic functions.
 */

/*!
 * \fn const QByteArray Crypto::random(int size)
 * Produce a random binary array using system random number generator.
 * \param size Number of random bytes requested.
 * \return a byte array of random values if successful.
 */

/*!
 * \fn bool Crypto::random(quint8 *bytes, int size)
 * Fills a section of memory with random binary data from the system random
 * number generator.
 * \param bytes Memory location to fill.
 * \param size Number of random bytes requested.
 * \return true if successful.
 */

/*!
 * \fn void Crypto::pad(QByteArray& data, int size)
 * Padds a data block to align data for a cipher.
 * \param data Binary data to pad
 * \param size Alignment size of padding for cipher.
 */

/*!
 * \fn void Crypto:unpad(QByteArray& data, int iv)
 * Remove padding that was added for cipher.  This also can be used to
 * remove a pre-pended iv.
 * \param data Binary data that was padded.
 * \param iv Pre-pended iv size.
 */

#endif
