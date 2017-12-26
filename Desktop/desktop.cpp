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

static Ui::MainWindow ui;

Desktop *Desktop::Instance = nullptr;

Desktop::Desktop(bool tray, bool reset) :
QMainWindow(), settings(CONFIG_FROM)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;

    if(reset)
        settings.clear();

#if defined(Q_OS_WIN)
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setIconSize(QSize(24, 24));         // uniform icon size...
#elif defined(Q_OS_MAC)
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setIconSize(QSize(24, 24));
#else
    setToolButtonStyle(Qt::ToolButtonFollowStyle);
#endif

    ui.setupUi(static_cast<QMainWindow *>(this));
    toolbar = new Toolbar(this, ui.toolBar);
    statusbar = new Statusbar(ui.centralwidget, ui.statusBar);
}

Desktop::~Desktop()
{
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
