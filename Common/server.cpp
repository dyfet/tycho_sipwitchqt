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

#include "compiler.hpp"
#include "server.hpp"
#include "logging.hpp"


#include <QSettings>
#include <QCommandLineParser>
#include <QAtomicInteger>
#include <QTimer>
#include <QDir>
#include <QLockFile>
#include <iostream>
#include <QHostInfo>
#include <QUuid>

#include <csignal>
#include <unistd.h>
#include <sys/stat.h>
#include <termios.h>
#include <paths.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>

#if defined(UNISTD_SYSTEMD)
#include <systemd/sd-daemon.h>
#endif

#ifdef Q_OS_MAC
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#endif

using namespace std;

typedef struct {
    QThread *thread;
    QThread::Priority priority;
    unsigned order;
} Context;

enum {
    // server events...
    SERVER_STARTUP = QEvent::User + 1,
    SERVER_SHUTDOWN,
    SERVER_RELOAD,
    SERVER_SUSPEND,
    SERVER_RESUME
};

class ServerEvent final : public QEvent
{
    Q_DISABLE_COPY(ServerEvent)

public:
    ServerEvent(int event, int code = 0) :
    QEvent(static_cast<QEvent::Type>(event)) {
        exitReason = code;
    }

    ~ServerEvent();

    int reason() const {
        return exitReason;
    }

private:
    int exitReason;
};

ServerEvent::~ServerEvent() {}

static bool notifySystemD = false;
static int exitReason = 0;
static bool staticConfig = false;
static QList<Context> contexts;
static unsigned maxOrder = 1;

Server *Server::Instance = nullptr;
QString Server::Uuid;
QVariantHash Server::CurrentConfig;
QVariantHash Server::DefaultConfig;
Server::ServerEnv Server::Env;
bool Server::DebugVerbose = false;
bool Server::RunAsService = false;
bool Server::RunAsDetached = false;
volatile Server::State Server::RunState = Server::START;

#ifdef Q_OS_MAC

// FIXME: This will require a new thread for cfrunloop to work!!

static io_connect_t ioroot = 0;
static io_object_t iopobj;
static IONotificationPortRef iopref;

static void iop_callback(void *ref, io_service_t ioservice, natural_t mtype, void *args) {
    Q_UNUSED(ioservice);
    Q_UNUSED(ref);

    switch(mtype) {
    case kIOMessageCanSystemSleep:
        IOAllowPowerChange(ioroot, (long)args);
        break;
    case kIOMessageSystemWillSleep:
        Server::suspend();
        IOAllowPowerChange(ioroot, (long)args);
        break;
    case kIOMessageSystemHasPoweredOn:
        Server::resume();
        break;
    default:
        break;
    }
}

static void iop_startup()
{
    void *ref = nullptr;

    if(ioroot != 0)
        return;

    ioroot = IORegisterForSystemPower(ref, &iopref, iop_callback, &iopobj);
    if(!ioroot) {
        Logging::err() << "Registration for power management failed";
        return;
    }
    else
        Logging::debug() << "Power management enabled";

    CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(iopref), kCFRunLoopCommonModes);
}

#endif

static void disableSignals()
{
    ::signal(SIGTERM, SIG_IGN);
    ::signal(SIGINT, SIG_IGN);
#ifdef SIGHUP
    ::signal(SIGHUP, SIG_IGN);
#endif
#ifdef SIGKILL
    ::signal(SIGKILL, SIG_IGN);
#endif
}

static void handleSignals(int signo)
{
    switch(signo) {
    case SIGINT:
        printf("\n");
        disableSignals();
        Server::shutdown(signo);
        break;
#ifdef SIGKILL
    case SIGKILL:
#endif
    case SIGTERM:
        disableSignals();
        Server::shutdown(signo);
        break;
#ifdef SIGHUP
    case SIGHUP:
        Server::reload();
        break;
#endif
    }
}

static void enableSignals()
{
#ifdef Q_OS_MAC
    iop_startup();
#endif

#ifdef SIGHUP
    ::signal(SIGHUP, handleSignals);
#endif
#ifdef SIGKILL
    ::signal(SIGKILL, handleSignals);
#endif
    ::signal(SIGINT, handleSignals);
    ::signal(SIGTERM, handleSignals);
}

Server::Server(int& argc, char **argv, QCommandLineParser& args, const QVariantHash& keypairs):
QObject(), app(argc, argv)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;

    foreach(auto key, keypairs.keys()) {
        if(key[0] == '_' || (key[0].isUpper() && key.indexOf('/') < 0))
            Env[key] = keypairs.value(key).toString().toLocal8Bit();
        else
            DefaultConfig[key] = keypairs.value(key);
    }

    args.process(app);

    if(args.isSet("detach") || getppid() == 1) {
        RunAsService = true;
        if(getenv("NOTIFY_SOCKET"))
            notifySystemD = true;
    }

#ifdef QT_NO_DEBUG
    DebugVerbose = args.isSet("debug");
#else
    DebugVerbose = true;
#endif

    QString cfgprefix = QCoreApplication::applicationName().toUpper() + "_";
    foreach(auto key, QProcess::systemEnvironment()) {
        if(!key[0].isUpper())
            continue;

        int pos = key.indexOf('=');
        if(pos < 2)
            continue;

        QString value = key.mid(pos + 1);
        key = key.left(pos);
        if(key.indexOf('_') > -1)
            continue;

        if(!Env[key].isEmpty() && (Env[key].left(2) != "--")) {
            continue;
        }

        Env[key] = value.toLocal8Bit();
    }

    foreach(auto key, keypairs.keys()) {
        QString value = keypairs.value(key).toString();
        if(value.left(2) == "--") {
            QString arg = value.mid(2);
            if(args.isSet(arg) || (Env[key].left(2) == "--"))
                Env[key] = args.value(arg).toLocal8Bit();
        }
    }

    // process uuid file...
    QFile uuidFile( QCoreApplication::applicationName() + ".uuid");

    if(!uuidFile.exists()) {
        QString uuid = QUuid::createUuid().toString();
        Uuid = uuid.mid(2);
        Uuid.chop(1);
        uuidFile.open(QIODevice::WriteOnly);
        QTextStream stream(&uuidFile);
        stream << Uuid << endl;
    }
    else {
        uuidFile.open(QIODevice::ReadOnly);
        QTextStream stream(&uuidFile);
        stream >> Uuid;
    }
    uuidFile.close();
    qDebug() << "Server uuid " << Uuid;

    reload();    // initial config...

    // paste in config option
    foreach(auto key, Env.keys()) {
        QString value = Env[key];
        if(value.left(2) == "%%")
            Env[key] = CurrentConfig[value.mid(2)].toString().toLocal8Bit();
    }

    Env[SYSTEM_HOSTNAME] = QHostInfo::localHostName().toUtf8();

    // Wouldn't it be great if Qt had an actual app starting signal??
    // This kinda fakes it in a way so that we can complete startup after
    // app exec() is called and main event loop is running.
    app.postEvent(Instance, new ServerEvent(SERVER_STARTUP));
}

Server::~Server()
{
    Instance = nullptr;
}

int Server::start(QThread::Priority priority)
{
    if(priority != QThread::InheritPriority)
        thread()->setPriority(priority);

    enableSignals();
    qDebug() << "Starting" << QCoreApplication::applicationName();
    // when server comes up logging is activated...
    Logging::init();
    emit aboutToStart();
    return app.exec();
}

bool Server::event(QEvent *evt)
{
    int id = static_cast<int>(evt->type());
    if(id < QEvent::User + 1)
        return QObject::event(evt);

    auto se = static_cast<ServerEvent *>(evt);
    switch(id) {
    case SERVER_STARTUP: // used to process post app exec startup
        qDebug() << "Server(STARTUP)";
        startup();
        return true;
    case SERVER_SHUTDOWN:
        qDebug() << "Server(SHUTDOWN)";
        exit(se->reason());
        return true;
    case SERVER_RELOAD:
        qDebug() << "Server(RELOAD)";
        reconfig();
        return true;
    case SERVER_SUSPEND:
        if(RunState != SUSPENDED) {
            emit aboutToSuspend();
            qDebug() << "Server(SUSPENDED)";
            RunState = SUSPENDED;
        }
        return true;
    case SERVER_RESUME:
        if(RunState == SUSPENDED) {
            emit aboutToResume();
            qDebug() << "Server(RESUME)";
            RunState = UP;
        }
        return true;
    }
    return false;
}

void Server::reconfig()
{
    // no need to reload if embedded config
    if(staticConfig)
        return;

    QVariantHash UpdatedConfig(DefaultConfig);
    const char *path = Env[SERVER_CONFIG].constData();
    if(QFile::exists(":/etc/service.conf")) {
        staticConfig = true;
        path = ":/etc/service.conf";
    }
    QSettings settings(path, QSettings::IniFormat);
    foreach(auto key, settings.allKeys()) {
        UpdatedConfig[key] = settings.value(key);
    }
    UpdatedConfig.squeeze();
    CurrentConfig = UpdatedConfig;

    if(RunState != START && RunState != DOWN)
        emit changeConfig(UpdatedConfig);
}

QThread *Server::findThread(const QString& name)
{
    foreach(auto context, contexts) {
        auto thread = context.thread;
        if(thread->objectName() == name)
            return thread;
    }
    return nullptr;
}

QThread *Server::createThread(const QString& name, unsigned order, QThread::Priority priority)
{
    Q_ASSERT(RunState == START);
    if(RunState != START)
        return nullptr;

    QThread *thread = new QThread;
    thread->setObjectName(name);
    Context context = {thread, priority, order};
    contexts << context;
    if(order >= maxOrder)
        maxOrder = ++order;
    return thread;
}

void Server::startup()
{
    if(RunState != START) {
        Logging::warn("??") << "Already started";
        return;
    }

    QString major = qApp->applicationVersion();
    QString minor = major.mid(major.indexOf('.') + 1);
    major = major.left(major.indexOf('.'));
    minor = minor.left(minor.indexOf('.'));
    unsigned abi = major.toUInt() * 100;
    abi += minor.toUInt();

    // TODO: currently race if came up suspended first
    RunState = UP;
    Logging::info("++") << "Service starting, version=" << app.applicationVersion() << ", abi=" << abi;

    // start managed threads
    for(unsigned order = 0; order < maxOrder; ++order) {
        for(int pos = 0; pos < contexts.count(); ++pos) {
            const Context &ctx = contexts.at(pos);
            if(ctx.order != order)
                continue;
            qDebug() << "Starting thread:" << ctx.thread->objectName();
            ctx.thread->start(ctx.priority);
        }
    }

    emit changeConfig(CurrentConfig);
    emit started();
    notify(SERVER_RUNNING, "Ready to process");
}

void Server::notify(SERVER_STATE state, const char *text)
{
#if defined(UNISTD_SYSTEMD)
    if(!notifySystemD)
        return;

    const char *cp;
    switch(state) {
    case SERVER_RUNNING:
        if(!text) {
            cp = "started";
            text = "";
        }
        else
            cp = "started:";

        sd_notifyf(0,
            "READY=1\n"
            "STATUS=%s%s\n"
            "MAINPID=%lu", cp, text, (unsigned long)getpid()
        );
        break;
    case SERVER_STOPPED:
        sd_notify(0, "STOPPING=1");
        break;
    default:
        break;
    }
#else
    Q_UNUSED(state);
    Q_UNUSED(text);
#endif
}

// since private, can only be triggered locally in our thread context.
void Server::exit(int reason)
{
    if(RunState == START) {
        QCoreApplication::exit(reason);
        return;
    }

    if(RunState == DOWN)
        return;

    // do service shutdown while in main thread context...
    RunState = DOWN;
    notify(SERVER_STOPPED);
    Logging::info("--") << "Service stopping, reason=" + QString::number(reason);
    app.processEvents();    // de-queue pending events
    emit aboutToFinish();

    // stop managed threads
    while(maxOrder--) {
        for(int pos = 0; pos < contexts.count(); ++pos) {
            const Context &ctx = contexts.at(pos);
            if(ctx.order != maxOrder)
                continue;
            qDebug() << "Stopping thread:" << ctx.thread->objectName();
            ctx.thread->quit();
            ctx.thread->wait();
            delete ctx.thread;
        }
    }

    emit finished();
    QCoreApplication::exit(reason);
}

void Server::suspend()
{
    QCoreApplication::postEvent(Instance, new ServerEvent(SERVER_SUSPEND));
}

void Server::resume()
{
    QCoreApplication::postEvent(Instance, new ServerEvent(SERVER_RESUME));
}

bool Server::shutdown(int reason)
{
    if(exitReason)
        return false;

    exitReason = reason;
    disableSignals();
    QCoreApplication::postEvent(Instance, new ServerEvent(SERVER_SHUTDOWN, reason));
    return true;
}

void Server::reload()
{
    Q_ASSERT(Instance != nullptr);
    if(RunState != START && RunState != DOWN && !exitReason)
        QCoreApplication::postEvent(Instance, new ServerEvent(SERVER_RELOAD));
    else {
        Instance->reconfig();
    }
}

