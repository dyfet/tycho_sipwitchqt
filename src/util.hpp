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

#include <QCoreApplication>
#include <QString>
#include <QDebug>
#include <QHostAddress>
#include <QCommandLineParser>

namespace Util {
    const QString localDomain();
    const QList<QHostAddress> bindAddress(const QString& hostId);
    const QList<QHostAddress> hostAddress(const QString& hostId);
    int hostPort(const QString& hostId);
    const QStringList controlOptions(const char **argv);
}

/*!
 * Some generic utility functions.
 * \file util.hpp
 * \ingroup Common
 */

/*!
 * \namespace Util
 * \brief Utility functions.
 * This namespace is typically used to create generic utility functions not necessarily
 * tied exclusively to our application.
 * \ingroup Common
 */
