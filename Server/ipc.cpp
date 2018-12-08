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
#include "ipc.hpp"
#include <QDir>
#include <QDebug>

#ifdef SYSV_MQUEUE
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#define MAX_SIZE 256

#if defined(POSIX_MQUEUE)

IPCServer *IPCServer::Instance = nullptr;

IPCServer::IPCServer(QThread::Priority priority) noexcept
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;

    path = "/sipwitchqt.ipc";
    struct mq_attr attr{};

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    ::remove(path.constData()); // just in case...
    ipc = mq_open(path.constData(), O_CREAT | O_RDONLY, 0660, &attr);
    auto thread = new QThread;
    thread->setObjectName("IPCServer");
    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &IPCServer::run);
    connect(this, &IPCServer::finished, thread, &QThread::quit);
    connect(this, &IPCServer::finished, this, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start(priority);
}

IPCServer::~IPCServer()
{
    mq_close(ipc);
    mq_unlink(path.constData());
    Instance = nullptr;
}

void IPCServer::run()
{
    debug() << "Message Queue Started";
    char buffer[MAX_SIZE];
    for(;;) {
        auto msize = mq_receive(ipc, buffer, MAX_SIZE, nullptr);
        if(msize < 2)
            break;
        QByteArray msg(buffer, static_cast<int>(msize));
        emit request(msg);
    }
    debug() << "Message Queue Stopped";
    emit finished();
}

IPCServer *IPCServer::start(QThread::Priority priority)
{
    if(Instance)
        return Instance;

    return new IPCServer(priority);
}

void IPCServer::stop()
{
    if(!Instance)
        return;

    char buf[1];
    auto ctrl = mq_open(Instance->path.constData(), O_WRONLY);
    mq_send(ctrl, buf, 1, 1);
    mq_close(ctrl);
    QThread::yieldCurrentThread();
}

#elif defined(SYSV_MQUEUE)

IPCServer *IPCServer::Instance = nullptr;

IPCServer::IPCServer(QThread::Priority priority) noexcept
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;

    ipc = msgget(ftok(".", 'u'), 0770 | IPC_CREAT);
    auto thread = new QThread;
    thread->setObjectName("IPCServer");
    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &IPCServer::run);
    connect(this, &IPCServer::finished, thread, &QThread::quit);
    connect(this, &IPCServer::finished, this, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start(priority);
}

IPCServer::~IPCServer()
{
    if(ipc > -1) {
        msgctl(ipc, IPC_RMID, nullptr);
        ::close(ipc);
        ipc = -1;
    }
    Instance = nullptr;
}

void IPCServer::run()
{
    debug() << "Message Queue Started";
    struct {
        long mtype;
        char mtext[MAX_SIZE];
    } buf;
    auto size = sizeof(buf) - sizeof(long);

    for(;;) {
        auto result = msgrcv(ipc, (struct msgbuf *)&buf, size, 1, 0);
        if(result < 0)
            break;
        QByteArray msg(buf.mtext, result);
        emit request(msg);
    }
    debug() << "Message Queue Stopped";
    emit finished();
}

IPCServer *IPCServer::start(QThread::Priority priority)
{
    if(Instance)
        return Instance;

    return new IPCServer(priority);
}

void IPCServer::stop()
{
    if(!Instance)
        return;

    if(Instance->ipc > -1) {
        auto ipc = Instance->ipc;
        Instance->ipc = -1;
        msgctl(ipc, IPC_RMID, nullptr);
        ::close(ipc);
    }
    QThread::yieldCurrentThread();
}

#else

IPCServer::~IPCServer() = default;

void IPCServer::run()
{
}

void IPCServer::stop()
{
}

IPCServer *IPCServer::start(QThread::Priority priority)
{
    Q_UNUSED(priority);
    return nullptr;
}

#endif

