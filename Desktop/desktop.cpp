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

Desktop::Desktop(bool tray, bool reset) :
QMainWindow(), settings(CONFIG_FROM)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;

    if(reset)
        settings.clear();

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

    if(!tray)
        return;

    trayIcon = new QSystemTrayIcon(this);
    if(trayIcon)
        trayMenu = new QMenu();
}

Desktop::~Desktop()
{
    if(control) {
        delete control;
        control = nullptr;
    }
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
    w.show();
    return app.exec();
}
