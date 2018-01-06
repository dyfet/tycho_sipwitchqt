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
#include "desktop.hpp"
#include "ui_desktop.h"

#include <QTranslator>
#include <QFile>

static Ui::MainWindow ui;

Desktop *Desktop::Instance = nullptr;
Desktop::state_t Desktop::State = Desktop::INITIAL;

Desktop::Desktop(bool tray, bool reset) :
QMainWindow(), listener(nullptr), storage(nullptr), settings(CONFIG_FROM)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;
    connected = false;

    if(reset) {
        Storage::remove();
        settings.clear();
    }

    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setIconSize(QSize(16, 16));

    control = new Control(this);

    ui.setupUi(static_cast<QMainWindow *>(this));
    toolbar = new Toolbar(this, ui.toolBar);
    statusbar = new Statusbar(ui.centralwidget, ui.statusBar);

    trayIcon = nullptr;
    trayMenu = dockMenu = appMenu = nullptr;

#if defined(Q_OS_MAC)
    dockMenu = new QMenu();
    qt_mac_set_dock_menu(dockMenu);
#endif

    login = new Login(this);
    sessions = new Sessions(this);
    ui.pagerStack->addWidget(login);
    ui.pagerStack->addWidget(sessions);
    ui.console->setHidden(true);

    if(tray)
        trayIcon = new QSystemTrayIcon(this);

    if(trayIcon) {
        trayMenu = new QMenu();
        trayIcon->setContextMenu(trayMenu);
        trayIcon->setIcon(QIcon(":/icons/offline.png"));
        trayIcon->setVisible(true);
        trayIcon->show();
    }

    login = new Login(this);
    ui.pagerStack->addWidget(login);

    if(Storage::exists()) {
        storage = new Storage();
        // hide();
        // authorizing();
        // attempt active login...
    }
    else {
        setWindowTitle("Welcome");
        warning("No local database active");
        ui.toolBar->hide();
        ui.pagerStack->setCurrentWidget(login);
        show();
    }
}

Desktop::~Desktop()
{
    if(listener) {
        listener->stop();
        listener = nullptr;
    }

    if(storage) {
        delete storage;
        storage = nullptr;
    }

    if(control) {
        delete control;
        control = nullptr;
    }
}

void Desktop::warning(const QString& text)
{
    ui.statusBar->setStyleSheet("color: orange;");
    ui.statusBar->showMessage(text);
}

void Desktop::error(const QString& text)
{
    ui.statusBar->setStyleSheet("color: red;");
    ui.statusBar->showMessage(text);
}

void Desktop::status(const QString& text)
{
    ui.statusBar->setStyleSheet("color: blue;");
    ui.statusBar->showMessage(text);
}

void Desktop::clear()
{
    ui.statusBar->showMessage("");
}


void Desktop::offline()
{
    if(trayIcon)
        trayIcon->setIcon(QIcon(":/icons/offline.png"));

    connected = false;
    listener = nullptr;
    State = OFFLINE;
}

void Desktop::initial()
{
    // storage = new Storage(login->credentials());
    ui.toolBar->show();
    ui.pagerStack->setCurrentWidget(sessions);
    authorizing();
}

void Desktop::authorizing()
{
    // listen()...
    if(trayIcon)
        trayIcon->setIcon(QIcon(":/icons/activate.png"));
    status("Connecting...");
    State = AUTHORIZING;
}

void Desktop::online()
{
    if(trayIcon)
        trayIcon->setIcon(QIcon(":/icons/online.png"));

    connected = true;
    State = ONLINE;
}

void Desktop::listen()
{
    Q_ASSERT(listener == nullptr);
    Q_ASSERT(storage != nullptr);

    listener = new Listener(storage->credentials());
    connect(listener, &Listener::starting, this, &Desktop::authorizing);
    connect(listener, &Listener::finished, this, &Desktop::offline);
    listener->start();
}

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationVersion(PROJECT_VERSION);
    QCoreApplication::setOrganizationName("Tycho Softworks");
    QCoreApplication::setOrganizationDomain("tychosoft.com");
    QCoreApplication::setApplicationName("Antisipate");
    QTranslator localize;

    QApplication app(argc, argv);
    QCommandLineParser args;
    Q_INIT_RESOURCE(desktop);

#if defined(Q_OS_MAC)
    localize.load(QLocale::system().name(), "manpager", "_",
        Args::exePath("../Translations"));
#elif defined(Q_OS_WIN)
    localize.load(QLocale::system().name(), "manpager", "_",
        Args::exePath("./Translations"));
#else
    localize.load(QLocale::system().name(), "manpager", "_",
        Args::exePath("../share/translations"));
#endif
    if(!localize.isEmpty())
        app.installTranslator(&localize);

    QFile style(":/styles/desktop.css");
    if(style.exists()) {
        style.open(QFile::ReadOnly);
        QString css = QLatin1String(style.readAll());
        style.close();
        qApp->setStyleSheet(css);
    }

    args.setApplicationDescription("SipWitchQt Desktop Client");
    Args::add(args, {
        {Args::HelpArgument},
        {Args::VersionArgument},
        {{"reset"}, "Reset Config"},
        {{"notray"}, "Disable tray icon"},
    });

    args.process(app);
    Desktop w(!args.isSet("notray"), args.isSet("reset"));
    return app.exec();
}
