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

/*!
 * Main include file for running the server.  This includes server default path
 * and config macros.
 * \file main.hpp
 */

/*!
 * A portable enterprise class sip server based on GNU SIP Witch.
 * SipWitchQt takes advantage of the Qt library to produce a database connected cross-
 * platform sip server.  The SipWitchQt architecture uses threads and signal slots to
 * isolate components and to serialize events so that shared locking can be avoided.
 * Blocking resources, such as database access, have their own thread and event loop.
 * Intercommunication between threaded components uses async Qt signal-slot over threads.
 * \n\n
 * The source tree is built for use with Qt Creator, and seperated into a number of
 * high level components thru .pri files.  These components include:
 * \section Archive
 * This is packaging support that mostly would be used by a project maintainer.
 * This functionality will also evently migrate to a git submodule.  This
 * includes things like the "publish", "archive", and "publish_and_archive"
 * make targets to package sources, documentation, and binaries.
 * \section Bootstrap
 * This is used principally to vendor libraries not found on MacOS or Microsoft
 * windows so that the project can be directly built on these platforms.   This
 * is provided thru a git submodule.
 * \section Common
 * This is a locally copied version of what will become LibServiceQt.  It
 * provides classes and compoments to write service deamons.  This includes
 * support for SystemD and NT services.  A control fifo, a logging system, a
 * thread manager, and support for orderly startup, shutdown, suspend, and
 * resume operations are included.  A config system and support for config
 * reload is also provided.
 * \section Database
 * This is the SipWitchQt database engine.  It provides an isolated database
 * service thread, and a query request system that can be used to issue timed
 * requests to the engine and receive callbacks on query completion.  This
 * includes low level query operations that are tied directly to SipWitchQt.
 * I may look at making this generic and using a derived class in the future.
 * \section Main
 * This is where most code specific to SipWitchQt will be found.  This may
 * derive classes from other generic top level components to create application
 * specific behaviors.
 * \section Stack
 * This is where all code and classes that touch eXosip2 and directly manage
 * the call stack are found.  This support is rather generic, and derived
 * thru a Manager class in main for SipWitchQt specific behaviors.
 * \mainpage SipWitchQt
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

