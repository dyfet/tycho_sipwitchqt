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

#ifndef IPC_HPP_
#define IPC_HPP_

#include "../Common/types.hpp"
#include "server.hpp"
#include <QThread>

#ifdef Q_OS_LINUX
#include <mqueue.h>
#define POSIX_MQUEUE
#endif

#ifdef Q_OS_MACX
#define SYSV_MQUEUE
#endif

class IPCServer final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(IPCServer)

public:
    static IPCServer *instance() {
        return Instance;
    }

    static IPCServer *start(QThread::Priority priority = QThread::InheritPriority);
    static void stop();

private:
#if defined(POSIX_MQUEUE)
    mqd_t ipc;
    UString path;
#elif defined(SYSV_MQUEUE)
    int ipc;
#endif

    explicit IPCServer(QThread::Priority priority) noexcept;
    ~IPCServer() final;

    static IPCServer *Instance;
    
signals:
    void request(UString message);
    void finished();

private slots:
    void run();
};

#endif
