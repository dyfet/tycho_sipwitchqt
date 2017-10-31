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
#include "../Common/control.hpp"
#include "../Common/system.hpp"
#include "../Common/logging.hpp"
#include "manager.hpp"
#include "main.hpp"

#include <iostream>
#include <QTextStream>
#include <QIODevice>
#include <QHostInfo>
#include <QUuid>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <sys/stat.h>
#endif

class Main final : public Control
{
    Q_DISABLE_COPY(Main)

public:
    Main(QCommandLineParser& args);

private:
    void onStartup() final;
    void onShutdown() final;
    const QString execute(const QStringList& args) final;
};

static int port;
static QList<QHostAddress> interfaces;
static unsigned protocols = Context::UDP | Context::TCP;

using namespace std;

Main::Main(QCommandLineParser& args) :
Control(SETTING)
{
    QStringList opt;

    if(args.isSet("set-aliases")) {
        if(setValue("aliases", args.positionalArguments()))
            ::exit(0);
        opt <<  QString("aliases");
        opt << args.positionalArguments();
        ::exit(request(opt));
    }
    else if(args.isSet("set-mode")) {
        if(args.positionalArguments().count() != 1) {
            cerr << "Invalid or missing mode setting." << endl;
            exit(90);
        }
        if(setValue("mode", args.positionalArguments().at(0)))
            ::exit(0);
        opt << QString("mode");
        opt << args.positionalArguments().at(0);
        ::exit(request(opt));
    }

    if(settings.value("values/mode").isNull())
        settings.setValue("values/mode", "day");
}

void Main::onStartup()
{
    qDebug() << "Control manager startup";
    Context::start(QThread::HighPriority);
}

void Main::onShutdown()
{
    qDebug() << "Control manager shutdown";
    Context::shutdown();
}

const QString Main::execute(const QStringList& args)
{
    if((args.count() > 1) && (args[0] == "aliases")) {
        QStringList list = args;
        list.removeFirst();
        storeValue("aliases", list);
        return "0";
    }
    else if((args.count() == 2) && (args[0] == "mode")) {
        storeValue("mode", args[1]);
        if(Server::state() != Server::START)
            Server::reload(); 
        return "0";
    }
    else
        return Control::execute(args);    
}   

#ifdef Q_OS_WIN

SERVICE_TABLE_ENTRY DetachedServices[] = {
    {(LPWSTR)TEXT(SERVICE_NAME),
        (LPSERVICE_MAIN_FUNCTION)System::detachService}, {NULL, NULL}
};

#endif

int main(int argc, char **argv)
{
    static bool detached = System::detach(argc, SERVICE_VARPATH, argv[0]);
    int exitcode = 0;

    QCoreApplication::setApplicationVersion(PROJECT_VERSION);
    QCoreApplication::setApplicationName(SERVICE_NAME);
    QCoreApplication::setOrganizationDomain(SERVICE_DOMAIN);
    QCoreApplication::setOrganizationName(SERVICE_ORG);

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
        {{"x", "debug"}, "Enable debug output"},
        {{"abort"}, "Force server abort"},
        {{"control"}, "Control service"},
        {{"reload"}, "Reload service config"},
        {{"reset"}, "Reset service"},
        {{"set-aliases"}, "Set aliases"},
        {{"set-mode"}, "Set mode (day, night, etc)"},
        {{"show-cache"}, "Show config cache"},
        {{"show-config"}, "Show server config"},
        {{"show-env"}, "Show server environment"},
        {{"show-values"}, "Show server persisted values"},
        {{"shutdown"}, "Shutdown service"},
        {{"status"}, "Show server status"},
    });

    //TODO: set argv[1] to nullptr, argc to 1 if Util::controlOptions count()
    Server server(detached, argc, argv, args, {
        {SERVER_NAME,       SERVICE_NAME},
        {SERVER_VERSION,    PROJECT_VERSION},
        {SERVER_PREFIX,        SERVICE_VARPATH},
        {SERVER_PIDFILE,    PIDFILE},
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

    // check base argument parsing
    if(Args::conflicting(args, {"status", "shutdown", "show-values", "show-env", "show-config", "show-cache", "set-mode", "set-aliases", "reset", "reload", "control", "abort"})) {
        cerr << "Conflicting command line options." << endl;
        exit(90);
    }

    if(Args::includes(args, {"status", "shutdown", "show-values", "show-env", "show-config", "show-cache", "set-mode", "set-aliases", "reload", "control", "abort"})) {
        if(Args::includes(args, {"address", "network", "port"})) {
            cerr << "Invalid option(s) used in offline mode." << endl;
            exit(90);
        }
        if(detached) {
            cerr << "Invalid option for detached server." << endl;
            exit(90);
        }
    }

    // verify and setup working directory
    if(strcmp(Server::sym(SYSTEM_PREFIX), Server::prefix())) {
        cerr << "Cannot access " << Server::prefix() << "." << endl;
        exit(90);
    }

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    QDir::current().mkdir("certs");
    QDir::current().mkdir("private");
#endif

    // validate global parsing results...
    port = atoi(Server::sym("PORT"));
    if(port < 100 || port > 65534 || port % 2)
        Logging::crit(95) << port << ": Invalid sip port value";

    interfaces = Util::bindAddress(Server::sym("ADDRESS"));
    if(interfaces.count() < 1)
        Logging::crit(95) << "No valid interfaces specified";

    if(args.isSet("reset"))
        Main::reset(SETTING);

    // create controller, load settings..

    Main controller(args);
    controller.options(args);

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
    qDebug() << "Exiting" << server.name();
    return exitcode;
}
