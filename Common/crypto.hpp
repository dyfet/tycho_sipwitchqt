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

#endif
