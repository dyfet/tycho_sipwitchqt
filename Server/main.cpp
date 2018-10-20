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

#include "../Common/compiler.hpp"
#include "../Common/args.hpp"
#include "../Common/crashhandler.hpp"
#include "server.hpp"
#include "output.hpp"
#include "manager.hpp"
#include "zeroconf.hpp"
#include "main.hpp"

#include <iostream>
#include <QTextStream>
#include <QIODevice>
#include <QHostInfo>
#include <QUuid>
#include <QProcess>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifdef Q_OS_MAC
void disable_nap();
#else
void disable_nap()
{
}
#endif

using namespace std;

namespace {
QString command = "none";
int exitcode = -1;
}

Main::Main(Server *server)
{
    connect(server, &Server::started, this, &Main::onStartup);
    connect(server, &Server::finished, this, &Main::onShutdown);
}

void Main::onStartup()
{
    debug() << "Control manager startup";
    Context::start(QThread::HighPriority);

#ifndef QT_NO_DEBUG_OUTPUT
    if(command == "none")
        return;

    QTimer::singleShot(1200, this, [=] {
        auto proc = new QProcess;
        connect(proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), [=](int code, QProcess::ExitStatus status) {
            Q_UNUSED(status);
            qDebug() << "Finished" << command << code;
            exitcode = code;
            Server::shutdown(exitcode);
        });
        connect(proc, &QProcess::errorOccurred, [=](QProcess::ProcessError error) {
            Q_UNUSED(error);
            qDebug() << "Failed" << command;
            exitcode = -1;
            Server::shutdown(-1);
        });
        qDebug() << "Start" << command;
        proc->setStandardOutputFile("test.out");
        proc->setStandardErrorFile("test.err");
        proc->start(command);
    });
#endif
}

void Main::onShutdown()
{
    debug() << "Control manager shutdown";
    Context::shutdown();
}

int main(int argc, char **argv)
{
    QCoreApplication::setApplicationVersion(PROJECT_VERSION);
    QCoreApplication::setApplicationName("sipwitchqt");
    QCoreApplication::setOrganizationDomain("tychosoft.com");
    QCoreApplication::setOrganizationName("Tycho Softworks");

    QCommandLineParser args;
    args.setApplicationDescription("Tycho SIP Witch Service");

    Args::add(args, {
        {{"A", "address"}, "Specify network interface to bind", "address", "%%address"},
        {{"D", "database"}, "Specify database driver", "database", "%%database"},
        {{"H", "host", "public"}, "Specify public host name", "host", "%%host"},
        {{"N", "network", "domain"}, "Specify network domain to serve", "name", "%%network"},
        {{"P", "port"}, "Specify network port to bind", "100-65534", "%%port"},
        {Args::HelpArgument},
        {Args::VersionArgument},
        {{"c", "config"}, "Specify config file", "file", SERVICE_CONF},
        {{"d", "detached"}, "Run as detached background daemon"},
        {{"f", "foreground"}, "Run as foreground daemon"},
#ifndef QT_NO_DEBUG_OUTPUT
        {{"t", "test"}, "Specify testing command", "command", "none"},
#endif
        {{"x", "debug"}, "Enable debug output"},
#ifndef QT_NO_DEBUG_OUTPUT
        {{"z", "zeroconfig"}, "Enable zeroconfig in debug"},
#endif
        {{"show-cache"}, "Show config cache"},
    });

#ifdef Q_OS_UNIX
    // if not root group, then use group read perms, else owner ownly...
    if(getegid())
        umask(027);
    else
        umask(077);
#endif

    QDir().mkdir(SERVICE_VARPATH);  // make sure we have a service path
    if(!QDir::setCurrent(SERVICE_VARPATH))
        crit(90) << SERVICE_VARPATH << ": cannot access";

    output() << "Prefix: " << SERVICE_VARPATH;

    //TODO: set argv[1] to nullptr, argc to 1 if Util::controlOptions count()
    Server server(argc, argv, args, {
        {SERVER_CONFIG,     "--config"},
        {CURRENT_DATABASE,  "--database"},
        {CURRENT_NETWORK,   "--network"},
        {CURRENT_HOSTNAME,  "--host"},
        {CURRENT_PORT,      "--port"},
        {CURRENT_ADDRESS,   "--address"},
        {DEFAULT_PORT,      5060},
        {DEFAULT_DATABASE,  "sqlite"},
        {DEFAULT_HOSTNAME,  QHostInfo::localHostName()},
        {DEFAULT_ADDRESS,   "any"},
        {DEFAULT_NETWORK,   Util::localDomain()},
    });

#ifndef QT_NO_DEBUG_OUTPUT
    if(args.isSet("test"))
        command = args.value("test");
#endif

    output() << "Config: " << server[SERVER_CONFIG];

    // validate global parsing results...
    quint16 port = server[CURRENT_PORT].toUShort();
    if(port < 100 || port > 65534 || port % 2)
        crit(95) << port << ": invalid sip port value";

    auto interfaces = Util::bindAddress(server[CURRENT_ADDRESS]);
    if(interfaces.count() < 1)
        crit(95) << "no valid interfaces specified";

    // create controller, load settings..
    auto zeroPort = port;

#ifndef QT_NO_DEBUG_OUTPUT
    if(!args.isSet("zeroconfig"))
        zeroPort = 0;
#endif

    Main controller(&server);
    Zeroconfig zeroconf(&server, zeroPort);

    Q_UNUSED(controller);
    Q_UNUSED(zeroconf);

    if(Server::isDetached() && CrashHandler::corefiles())
            CrashHandler::installHandlers();

    // setup our contexts...allow registration
    disable_nap();

    unsigned mask = Context::UDP | Context::TCP | Context::Allow::REGISTRY | Context::Allow::REMOTE;

    Manager::create(interfaces, port, mask);

    // create managers and start server...
    Manager::init(2);
    Database::init(3);

    if(Util::dbIsFile(server[CURRENT_DATABASE].toUpper()))
        Authorize::init(0);
    else
        Authorize::init(6);

    auto result = server.start();
    if(exitcode == -1)
        exitcode = result;

    //config.sync();
    debug() << "Exiting " << QCoreApplication::applicationName();
    return exitcode;
}
