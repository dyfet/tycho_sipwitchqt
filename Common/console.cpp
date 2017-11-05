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

#include "console.hpp"
#include "server.hpp"
#include <QCoreApplication>

static QMutex lock;

crit::~crit()
{
    if(!exitCode)
        return;

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
    out << buffer << endl;
    out.flush();
}


error::~error()
{
    if(Server::isDetached())
        return;

    QTextStream out(stderr);
    QMutexLocker locker(&lock);
    if(prefix)
    out << "** " << QCoreApplication::applicationName() << ": ";
    out << buffer << endl;
    out.flush();
}


