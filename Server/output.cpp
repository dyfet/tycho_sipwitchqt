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
 */

#include "output.hpp"
#include "server.hpp"
#include <QCoreApplication>
#include <QMutexLocker>

#ifndef Q_OS_WIN
#include <syslog.h>
#endif

namespace {
QMutex lock;
}

crit::~crit()
{
    if(!exitCode)
        return;

#ifndef Q_OS_WIN
    if(Server::isService())
        ::syslog(LOG_CRIT, "%s", buffer.toUtf8().constData());
#endif

    if(!Server::isDetached()) {
        QMutexLocker locker(&lock);
        QTextStream out(stderr);
        out << "** " << QCoreApplication::applicationName() << ": " << buffer << endl;
        out.flush();
    }
    ::exit(exitCode);
}

output::~output()
{
    if(Server::isDetached())
        return;

    QTextStream out(stdout);
    QMutexLocker locker(&lock);
    out << buffer << endl;
    out.flush();
}

debug::~debug()
{
    if(Server::isDetached() || !Server::verbose())
        return;

    QTextStream out(stdout);
    QMutexLocker locker(&lock);
    out << "-- " << buffer << endl;
    out.flush();
}

info::~info()
{
#ifndef Q_OS_WIN
    if(Server::isService())
        ::syslog(LOG_INFO, "%s", buffer.toUtf8().constData());
#endif

    if(Server::isDetached())
        return;

    QTextStream out(stdout);
    QMutexLocker locker(&lock);
    out << "%% " << buffer << endl;
    out.flush();
}

notice::~notice()
{
#ifndef Q_OS_WIN
    if(Server::isService())
        ::syslog(LOG_NOTICE, "%s", buffer.toUtf8().constData());
#endif

    if(Server::isDetached())
        return;

    QTextStream out(stdout);
    QMutexLocker locker(&lock);
    out << "== " << buffer << endl;
    out.flush();
}

warning::~warning()
{
#ifndef Q_OS_WIN
    if(Server::isService())
        ::syslog(LOG_WARNING, "%s", buffer.toUtf8().constData());
#endif

    if(Server::isDetached())
        return;

    QTextStream out(stderr);
    QMutexLocker locker(&lock);
    out << "## " << buffer << endl;
    out.flush();
}

error::~error()
{
#ifndef Q_OS_WIN
    if(Server::isService())
        ::syslog(LOG_ERR, "%s", buffer.toUtf8().constData());
#endif

    if(Server::isDetached())
        return;

    QTextStream out(stderr);
    QMutexLocker locker(&lock);
    out << "** " << buffer << endl;
    out.flush();
}


