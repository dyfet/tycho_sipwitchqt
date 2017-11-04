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

#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include <QHash>
#include <QString>
#include <QProcess>
#include <QThread>
#include <QObject>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QEvent>
#include <QVariant>
#include <QMutex>

#define SYSTEM_PREFIX   "PWD"
#define SYSTEM_HOME     "HOME"
#define SYSTEM_USER     "LOGNAME"
#define SYSTEM_HOSTNAME "HOSTNAME"

#define SERVER_LOGFILE  "_logfile"
#define SERVER_PIDFILE  "_pidfile"
#define SERVER_CONFIG   "SERVICE_CONFIG"
#define SERVER_NAME     "SERVICE_NAME"
#define SERVER_VERSION  "SERVICE_VERSION"
#define SERVER_PREFIX      "SERVICE_PREFIX"

typedef enum {
    SERVER_RUNNING,
    SERVER_WATCHDOG,
    SERVER_STOPPED
} SERVER_STATE;

class Server final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Server)

    friend class Control;

public:
    typedef enum { START, UP, SUSPENDED, DOWN } State;

    Server(bool detached, int& argc, char **argv, QCommandLineParser &args, const QVariantHash &keypairs);
    ~Server();

    int start(QThread::Priority priority = QThread::InheritPriority);

    static QThread *createThread(const QString& name, unsigned order = 0, QThread::Priority = QThread::InheritPriority);

    static QThread *findThread(const QString& name);

    inline static Server *instance(void) {
        Q_ASSERT(Instance != nullptr);
        return Instance;
    }

    inline static State state(void) {
        return RunState;
    }

    inline static bool verbose(void) {
        return DebugVerbose;
    }

    inline static QString uuid(void) {
        return Uuid;
    }

    inline static bool isService(void) {
        return RunAsService;
    }

    inline static const char *sym(const QString key) {
        return Env[key].constData();
    }

    inline static const char *name() {
        return sym(SERVER_NAME);
    }

    inline static const char *prefix() {
        return sym(SERVER_PREFIX);
    }

    inline static const QHash<QString, QByteArray> env(void) {
        return Env;
    }

    static bool detach(int argc, const char *path);
    static void notify(SERVER_STATE state, const char *text = nullptr);
    static bool shutdown(int exitcode);
    static void reload();
    static void suspend();
    static void resume();

private:
    typedef QHash<QString, QByteArray> ServerEnv;

    QCoreApplication app;

    static Server *Instance;
    static QString Uuid;
    static QVariantHash CurrentConfig, DefaultConfig;
    static ServerEnv Env;
    static bool DebugVerbose;
    static bool RunAsService;
    volatile static State RunState;

    bool event(QEvent *evt) final;

    void startup();
    void exit(int reason = 0);
    void reconfig();

signals:
    void aboutToStart();
    void aboutToFinish();
    void aboutToSuspend();
    void aboutToResume();
    void changeConfig(const QVariantHash& cfg);
    void started();
    void finished();
};

/*!
 * Daemon service management core.
 * \file server.hpp
 */

/*!
 * \class Server
 * \brief The core daemon service class.
 * This is used to create and manage a service daemon under Qt.  On Posix
 * systems, the deamon is supported thru a double fork().  Support is also
 * included to operate as a service under Linux SystemD.  On Windows
 * support is provided for operating as a NT service.
 *
 * The service daemon also manages ordered startup and shutdown of long term
 * service persistent threads, loading of a config file, and service
 * restart processing.  For Posix systems sighup can be used as a service
 * reload request.  Suspend-resume support is also integrated.
 *
 * A number of signals are emitted so that other components can be made
 * aware of the running state of the service and to update configurations.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
