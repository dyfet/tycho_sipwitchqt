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

#include "../Common/compiler.hpp"
#include "../Common/args.hpp"
#include "../Common/server.hpp"
#include "../Common/crashhandler.hpp"
#include "../Common/logging.hpp"
#include "../Common/console.hpp"
#include "manager.hpp"
#include "main.hpp"

#include <iostream>
#include <QTextStream>
#include <QIODevice>
#include <QHostInfo>
#include <QUuid>

static int port;
static QList<QHostAddress> interfaces;
static unsigned protocols = Context::UDP | Context::TCP;

using namespace std;

Main::Main(Server *server)
{
    connect(server, &Server::started, this, &Main::onStartup);
    connect(server, &Server::finished, this, &Main::onShutdown);
}

Main::~Main()
{
}

void Main::onStartup()
{
    debug() << "Control manager startup";
    Context::start(QThread::HighPriority);
}

void Main::onShutdown()
{
    debug() << "Control manager shutdown";
    Context::shutdown();
}

int main(int argc, char **argv)
{
    int exitcode = 0;

    QCoreApplication::setApplicationVersion(PROJECT_VERSION);
    QCoreApplication::setApplicationName("sipwitchqt");
    QCoreApplication::setOrganizationDomain("tychosoft.com");
    QCoreApplication::setOrganizationName("Tycho Softworks");

    QCommandLineParser args;
    args.setApplicationDescription("Tycho SIP Witch Service");

    Args::add(args, {
        {{"A", "address"}, "Specify network interface to bind", "address", "%%address"},
        {{"C", "config"}, "Specify config file", "file", SERVICE_CONF},
        {{"H", "host", "public"}, "Specify public host name", "host", "%%host"},
        {{"N", "network", "domain"}, "Specify network domain to serve", "name", "%%network"},
        {{"P", "port"}, "Specify network port to bind", "100-65534", "%%port"},
        {Args::HelpArgument},
        {Args::VersionArgument},
        {{"d", "detached"}, "Run as detached background daemon"},
        {{"f", "foreground"}, "Run as foreground daemon"},
        {{"x", "debug"}, "Enable debug output"},
        {{"show-cache"}, "Show config cache"},
    });

    if(!QDir::setCurrent(SERVICE_VARPATH))
        crit(90) << SERVICE_VARPATH << ": cannot access";

    output() << "Prefix: " << SERVICE_VARPATH;

    //TODO: set argv[1] to nullptr, argc to 1 if Util::controlOptions count()
    Server server(argc, argv, args, {
        {SERVER_LOGFILE,    LOGFILE},
        {SERVER_CONFIG,     "--config"},
        {CURRENT_NETWORK,   "--network"},
        {CURRENT_HOSTNAME,  "--host"},
        {CURRENT_PORT,      "--port"},
        {CURRENT_ADDRESS,   "--address"},
        {DEFAULT_PORT,      5060},
        {DEFAULT_HOSTNAME,  QHostInfo::localHostName()},
        {DEFAULT_ADDRESS,   "any"},
        {DEFAULT_NETWORK,   Util::localDomain()},
    });

#if defined(Q_OS_MAC)
    QDir::current().mkdir("certs");
    QDir::current().mkdir("private");
#endif

    // validate global parsing results...
    port = atoi(Server::sym("PORT"));
    if(port < 100 || port > 65534 || port % 2)
        crit(95) << port << ": invalid sip port value";

    interfaces = Util::bindAddress(Server::sym("ADDRESS"));
    if(interfaces.count() < 1)
        crit(95) << "no valid interfaces specified";

    // create controller, load settings..

    Main controller(&server);
    Q_UNUSED(controller);

    if(Server::isDetached() && CrashHandler::corefiles())
            CrashHandler::installHandlers();

    // setup our contexts...allow registration

    unsigned mask = protocols;
    mask |= Context::Allow::REGISTRY |\
            Context::Allow::REMOTE;

    Manager::create(interfaces, port, mask);

    // create managers and start server...
    Database::init(2);
    Manager::init(3);
    exitcode = server.start();

    //config.sync();
    debug() << "Exiting " << QCoreApplication::applicationName();
    return exitcode;
}
