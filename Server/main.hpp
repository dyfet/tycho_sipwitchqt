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

#include <QtDebug>
#include <QString>
#include <QChar>
#include <QCoreApplication>
#include <QTimer>
#include <QTime>
#include <QThread>
#include <QHash>
#include <QDir>

#ifdef XYZ // needed to force conditional highlighting in qtcreator! Yuck!
Q_UNUSED(xyz)
#endif

// Testdata mode is a server work prefix, rest relative to it
#if defined(PROJECT_PREFIX)
#define SERVICE_CONF "service.conf"
#define LOGFILE "service.log"
#else
#define SERVICE_CONF    SERVICE_ETCPATH "//" SERVICE_NAME ".conf"
#define LOGFILE SERVICE_LOGPATH "/" SERVICE_NAME ".log"
#endif

#define TRACEIT "trace.log"
#define SETTING "config.db"
#define DATABASE "local.db"

#if defined(Q_OS_LINUX)
#define MIN_USER_UID    1000
#else
#define MIN_USER_UID    500
#endif

#define CURRENT_ADDRESS     "ADDRESS"
#define CURRENT_NETWORK     "NETWORK"
#define CURRENT_HOSTNAME    "HOST"
#define CURRENT_PORT        "PORT"
#define DEFAULT_HOSTNAME    "host"
#define DEFAULT_ADDRESS     "address"
#define DEFAULT_NETWORK     "network"
#define DEFAULT_PORT        "port"

class Main final : public QObject
{
    Q_DISABLE_COPY(Main)

public:
    Main(Server *server);
    ~Main();

private slots:
    void onStartup();
    void onShutdown();
};

/*!
 * \mainpage SipWitchQt
 * A portable enterprise class sip server based on GNU SIP Witch.
 * SipWitchQt takes advantage of the Qt library to produce a database connected cross-
 * platform sip server.  The SipWitchQt architecture uses threads and signal slots to
 * isolate components and to serialize events so that shared locking can be avoided.
 * Blocking resources, such as database access, have their own thread and event loop.
 * Intercommunication between threaded components uses async Qt signal-slot over threads.
 * \author David Sugar <tychosoft@gmail.com>
 */

/*!
 * Main include file for running the server.  This includes server default path
 * and config macros.
 * \file main.hpp
 */
