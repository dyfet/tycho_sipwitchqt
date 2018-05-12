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

#include "util.hpp"
#include <QHostInfo>
#include <QNetworkInterface>
#ifndef Q_OS_WIN
#include <sys/socket.h>
#endif

const QString Util::localDomain()
{
    QString domain = QHostInfo::localDomainName();
    if(domain.isEmpty()) {
        QString host = QHostInfo::localHostName();
        int pos = host.indexOf(".");
        if(pos < 0)
            domain = "localdomain";
        else
            domain = host.mid(++pos);
    }
    return domain;
}

const QList<QHostAddress> Util::hostAddress(const QString& hostId)
{
    int pos = hostId.indexOf(":");
    QString bindId = hostId;

    if(pos > 0)
        bindId = hostId.left(pos);

    QList<QHostAddress> list;
    if(bindId != "none") {
        auto iface = QNetworkInterface::interfaceFromName(bindId);
        if(iface.isValid()) {
            auto entries = iface.addressEntries();
            foreach(auto entry, entries) {
                list << entry.ip();
            }
        }
    }
    return list;
}

int Util::hostPort(const QString& hostId)
{
    int pos = hostId.indexOf(":");
    if(pos < 1)
        return 0;
    pos = hostId.mid(pos).toInt();
    if((pos < 0) || (pos > 65535))
        pos = 0;
    return pos;
}

const QList<QHostAddress> Util::bindAddress(const QString& hostId)
{
    QList<QHostAddress> list;
    if(hostId == "*" || hostId == "all") {
#ifdef AF_INET6
        list << QHostAddress(QHostAddress::AnyIPv4);
        list << QHostAddress(QHostAddress::AnyIPv6);
#else
        list << QHostAddress(QHostAddress::Any);
#endif
    }
    else if(hostId == "any6") {
#ifdef AF_INET6
        list << QHostAddress(QHostAddress::AnyIPv6);
#else
        list << QHostAddress(QHostAddress::Any);
#endif
    }
    else if(hostId == "any4") {
        list << QHostAddress(QHostAddress::AnyIPv4);
    }
    else if(hostId == "any") {
        list << QHostAddress(QHostAddress::Any);
    }
    else {
        auto iface = QNetworkInterface::interfaceFromName(hostId);
        if(iface.isValid()) {
            auto entries = iface.addressEntries();
            foreach(auto entry, entries) {
                list << entry.ip();
            }
        }
        else {
            QHostInfo hostinfo = QHostInfo::fromName(hostId);
            list = hostinfo.addresses();
        }
    }
    return list;
}

quint16 Util::portNumber(const char *cp)
{
    if(!cp)
        return 0;

    return static_cast<quint16>(atoi(cp));
}

unsigned Util::currentDay(const QDateTime& when)
{
    auto secs = when.toMSecsSinceEpoch() / 1000l;
    secs += when.offsetFromUtc();
    return static_cast<unsigned>(secs / 86400l);
}

unsigned Util::untilTomorrow(const QDateTime& when)
{
    auto secs = when.toMSecsSinceEpoch() / 1000l;
    secs += when.offsetFromUtc();
    return static_cast<unsigned>(86400 - (secs % 86400l));
}

qlonglong Util::timestampKey(const QDateTime& when, unsigned sequence)
{
    auto secs = when.toMSecsSinceEpoch() / 1000l;
    secs *= 10000;
    secs += (sequence % 10000);
    return secs;
}
