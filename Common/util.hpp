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

#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <QCoreApplication>
#include <QString>
#include <QDebug>
#include <QHostAddress>
#include <QCommandLineParser>
#include <QDateTime>

namespace Util {
    quint16 portNumber(const char *cp);
    const QString localDomain();
    const QList<QHostAddress> bindAddress(const QString& hostId);
    const QList<QHostAddress> hostAddress(const QString& hostId);
    int hostPort(const QString& hostId);
    unsigned currentDay(const QDateTime& when = QDateTime::currentDateTime());
    unsigned untilTomorrow(const QDateTime& when = QDateTime::currentDateTime());
}

/*!
 * Some generic utility functions.
 * \file util.hpp
 */

/*!
 * \namespace Util
 * \brief Utility functions.
 * This namespace is typically used to create generic utility functions not necessarily
 * tied exclusively to our application.
 */

/*!
 * \fn portNumber(const char *cp)
 * Converts a text string into a valid ip port number.
 * \param cp text string for port number.
 * \return 0 if fails, else port number.
 *
 * \fn localDomain()
 * Return effective local domain of your machine.
 * \return domain name string.
 *
 * \fn bindAddress(const QString& hostId)
 * Create a list of possible interface bind addresses for a given hostname
 * or wildcard string.
 * \param hostId Name of host or "*"
 * \return list of interface addresses possible.
 *
 * \fn hostAddress(const QString& hostId)
 * Return list of possible addresses for a host:port style host string.
 * \param hostId Name of host (and :port)
 * \return list of contact addresses possible.
 *
 * \fn hostPort(const QString& hostId)
 * Returns port number of a host:port style host id.
 * \param hostId Name of host (and :port)
 * \return port address or 0 if none.
 *
 * \fn currentDay(const QDateTime& when)
 * Returns the current day number since the epoch based on current timezone.
 * This is often used to determine if two timestamps are from the same day.
 * \param when Timestamp to get day from.
 * \return number of days since the "epoch".
 *
 * \fn untilTomorrow(const QDateTime& when)
 * Returns number of seconds remaining until midnight rollover to the next
 * day, for current timezone.  Sometimes used to determine timer wakeups.
 * \param when Timestap to check.
 * \return number of seconds until next day.
 */
#endif
