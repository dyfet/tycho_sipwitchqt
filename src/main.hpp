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

#define CONFIG_STR1(x) #x
#define CONFIG_STR(x) CONFIG_STR1(x)

#define SERVICE_DOMAIN  "tychosoft.com"
#define SERVICE_ORG     "Tycho Softworks"
#define	SERVICE_NAME	CONFIG_STR(PROJECT_NAME)
#define SERVICE_VERSION CONFIG_STR(PROJECT_VERSION)
#define SERVICE_COPYRIGHT CONFIG_STR(PROJECT_COPYRIGHT)
#define SERVICE_READY   SERVICE_NAME " started: Ready to process calls"

// Testdata mode is a server work prefix, rest relative to it
#if defined(PROJECT_PREFIX)
#define SERVICE_VARPATH CONFIG_STR(PROJECT_PREFIX)
#define SERVICE_LIBEXEC CONFIG_STR(PROJECT_PREFIX)
#define SERVICE_DATADIR CONFIG_STR(PROJECT_PREFIX)
#define SERVICE_CONF "service.conf"
#define LOGFILE "service.log"
#define PIDFILE "service.pid"
#else
#define SERVICE_USRPATH CONFIG_STR(UNISTD_PREFIX)
#define SERVICE_VARPATH CONFIG_STR(UNISTD_VARPATH) "/" SERVICE_NAME
#define SERVICE_LIBEXEC SERVICE_USRPATH "/libexec/" SERVICE_NAME
#define SERVICE_DATADIR SERVICE_USRPATH "/share/" SERVICE_NAME
#define SERVICE_CONF    CONFIG_STR(UNISTD_ETCPATH) "//" SERVICE_NAME ".conf"
#define LOGFILE CONFIG_STR(UNISTD_LOGPATH) "/" SERVICE_NAME ".log"
#define PIDFILE "pidfile"
#endif

#define TRACEIT "trace.log"
#define SETTING "settings.db"

#if defined(Q_OS_LINUX)
#define MIN_USER_UID    1000
#else
#define MIN_USER_UID    500
#endif

#define CURRENT_ADDRESS     "ADDRESS"
#define CURRENT_NETWORK     "NETWORK"
#define CURRENT_PORT        "PORT"
#define	CURRENT_UUID		"UUID"
#define DEFAULT_ADDRESS     "address"
#define DEFAULT_NETWORK     "network"
#define DEFAULT_PORT        "port"

