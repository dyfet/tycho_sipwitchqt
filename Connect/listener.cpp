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

#include "listener.hpp"

#define EVENT_TIMER 500l    // 500ms...

Listener *Listener::Instance = nullptr;

Listener::Listener(const UString& address, quint16 port) :
QObject(), active(true)
{
    QThread *thread = new QThread;
    this->moveToThread(thread);
    serverAddress = address;
    serverPort = port;

    connect(thread, &QThread::started, this, &Listener::run);
    connect(this, &Listener::finished, thread, &QThread::quit);
    connect(this, &Listener::finished, this, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start(QThread::HighPriority);
}

void Listener::run()
{
    while(active) {
        int s = EVENT_TIMER / 1000l;
        int ms = EVENT_TIMER % 1000l;
        QThread::msleep(EVENT_TIMER);       // until listener code built up
    }
    emit finished();
}

void Listener::start(const UString& address, quint16 port)
{
    Q_ASSERT(Instance == nullptr);
    Instance = new Listener(address, port);
}

void Listener::stop()
{
    if(Instance) {
        Instance->active = false;
        Instance = nullptr;
        QThread::msleep(100);
    }
}
