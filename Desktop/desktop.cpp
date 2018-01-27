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
#include <QCloseEvent>

#ifdef Q_OS_MAC
#include <objc/objc.h>
#include <objc/message.h>

void set_dock_icon(const QIcon& icon);
void set_dock_label(const QString& text);

static bool dock_click_handler(::id self, SEL _cmd, ...)
{
    Q_UNUSED(self)
    Q_UNUSED(_cmd)

    auto desktop = Desktop::instance();
    QMetaObject::invokeMethod(desktop, "dock_clicked", Qt::QueuedConnection);
    return false;
}
#endif

static Ui::MainWindow ui;

Desktop *Desktop::Instance = nullptr;
Desktop::state_t Desktop::State = Desktop::INITIAL;
QVariantHash Desktop::Credentials;

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

    appearance = settings.value("appearance", "Default").toString();
    control = new Control(this);

    ui.setupUi(static_cast<QMainWindow *>(this));
    toolbar = new Toolbar(this, ui.toolBar);
    statusbar = new Statusbar(ui.centralwidget, ui.statusBar);

    trayIcon = nullptr;
    trayMenu = dockMenu = appMenu = nullptr;

#if defined(Q_OS_MAC)
    dockMenu = new QMenu();
    dockMenu->addAction(ui.trayDnd);
    dockMenu->addAction(ui.trayAway);
    qt_mac_set_dock_menu(dockMenu);

    auto inst = objc_msgSend(reinterpret_cast<objc_object *>(objc_getClass("NSApplication")), sel_registerName("sharedApplication"));

    if(inst) {
        auto delegate = objc_msgSend(inst, sel_registerName("delegate"));
        auto handler = reinterpret_cast<Class>(objc_msgSend(delegate, sel_registerName("class")));

        SEL shouldHandle = sel_registerName("applicationShouldHandleReopen:hasVisibleWindows:");
        if (class_getInstanceMethod(handler, shouldHandle)) {
            if (class_replaceMethod(handler, shouldHandle, reinterpret_cast<IMP>(dock_click_handler), "B@:")) {
                set_dock_icon(QIcon(":/icons/offline.png"));
                tray = false;
            }
        }
        else {
            if (class_addMethod(handler, shouldHandle, reinterpret_cast<IMP>(dock_click_handler), "B@:")) {
                tray = false;
                set_dock_icon(QIcon(":/icons/offline.png"));
            }
        }
    }

    if(tray)
        set_dock_icon(QIcon(":/icons/idle.png"));

#endif

    login = new Login(this);
    sessions = new Sessions(this);
    options = new Options(this);
    phonebook = new Phonebook(this);
    ui.pagerStack->addWidget(login);
    ui.pagerStack->addWidget(sessions);
    ui.pagerStack->addWidget(phonebook);
    ui.pagerStack->addWidget(options);
    ui.console->setHidden(true);

    connect(ui.actionSettings, &QAction::triggered, this, &Desktop::showOptions);
    connect(ui.actionSessions, &QAction::triggered, this, &Desktop::showSessions);
    connect(ui.actionContacts, &QAction::triggered, this, &Desktop::showPhonebook);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Desktop::shutdown);
    connect(qApp, &QGuiApplication::applicationStateChanged, this, &Desktop::appState);

    if(tray)
        trayIcon = new QSystemTrayIcon(this);

    if(trayIcon) {
        trayMenu = new QMenu();
        trayMenu->addAction(ui.trayDnd);
        trayMenu->addAction(ui.trayAway);
        trayMenu->addAction(ui.trayQuit);

        trayIcon->setContextMenu(trayMenu);
        trayIcon->setIcon(QIcon(":/icons/offline.png"));
        trayIcon->setVisible(true);
        trayIcon->show();

        connect(trayIcon, &QSystemTrayIcon::activated, this, &Desktop::trayAction);
        connect(ui.trayQuit, &QAction::triggered, qApp, &QCoreApplication::quit);
    }

    ui.trayDnd->setChecked(true);   // FIXME: testing...
    // connect(ui.trayDnd, &QAction::triggered, this, &Desktop::trayDnd);
    connect(ui.trayAway, &QAction::triggered, this, &Desktop::trayAway);

    login = new Login(this);
    ui.pagerStack->addWidget(login);

    if(Storage::exists()) {
        storage = new Storage("");
        showSessions();
        restoreGeometry(settings.value("geometry").toByteArray());
        show();                             // FIXME: hide
        listen(storage->credentials());
    }
    else {
        ui.trayAway->setEnabled(false);
        setWindowTitle("Welcome");
        warning("No local database active");
        ui.toolBar->hide();
        ui.pagerStack->setCurrentWidget(login);
        login->enter();
        show();
        settings.setValue("geometry", saveGeometry());
    }
}

Desktop::~Desktop()
{
    shutdown();
}

void Desktop::closeEvent(QCloseEvent *event)
{
    if(isVisible())
        settings.setValue("geometry", saveGeometry());

    // close to tray, if we are active...
    if(trayIcon && trayIcon->isVisible() && listener) {
        hide();
        event->ignore();
    }
}

void Desktop::shutdown()
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

void Desktop::setState(state_t state)
{
    // FIXME: Eventually adjusted by appearance
    QString prefix = ":/icons/";
    QString icon;

    State = state;
    switch(State) {
    case AUTHORIZING:
        icon = prefix + "activate.png";
        break;
    case ONLINE:
        icon = prefix + "online.png";
        break;
    default:
        icon = prefix + "offline.png";
        break;
    }

    if(trayIcon)
        trayIcon->setIcon(QIcon(icon));
    else
#if defined(Q_OS_MAC)
        set_dock_icon(QIcon(icon));
#else
        setWindowIcon(QIcon(icon));
#endif
}

void Desktop::showOptions()
{
    ui.pagerStack->setCurrentWidget(options);
    options->enter();
}

void Desktop::showSessions()
{
    ui.pagerStack->setCurrentWidget(sessions);
    sessions->enter();
}

void Desktop::showPhonebook()
{
    ui.pagerStack->setCurrentWidget(phonebook);
    phonebook->enter();
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

void Desktop::appState(Qt::ApplicationState state)
{
    // Actve, Inactive related to front/back...
    qDebug() << "*** STATE " << state;
}

void Desktop::dock_clicked()
{
    trayAction(QSystemTrayIcon::MiddleClick);
}

void Desktop::trayAway()
{
    if(connected) {
        status(tr("disconnect"));
        offline();
    }
    else
        authorizing();
}

void Desktop::trayAction(QSystemTrayIcon::ActivationReason reason)
{
    switch(reason) {
    case QSystemTrayIcon::MiddleClick:
#ifndef Q_OS_MAC
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
#endif
        if(isMinimized())
            break;
        if(isHidden())
            show();
        else
            hide();
        break;
    default:
        break;
    }
}

void Desktop::failed(int error_code)
{
    switch(error_code) {
    case SIP_CONFLICT:
        error(tr("Label already used"));
        if(isLogin())
            login->badLabel();
        break;
    case SIP_FORBIDDEN:
    case SIP_UNAUTHORIZED:
        error(tr("Authorizartion denied"));
        if(isLogin())
            login->badPassword();
        break;
    case SIP_INTERNAL_SERVER_ERROR:
    case 666:
        error(tr("Cannot reach server"));
        if(isLogin())
            login->badIdentity();
        break;
    default:
        error(tr("Unknown server failure"));
        if(isLogin())
            login->badIdentity();
        break;
    }
    offline();
}

void Desktop::offline()
{
    // if already offline, we can ignore...
    if(!listener)
        return;

    // could return from authorizing to offline...
    ui.trayAway->setText(tr("Connect"));
    ui.trayAway->setEnabled(!isLogin());

    if(connected) {
        connected = false;
        emit online(connected);
    }
    listener = nullptr;
    setState(OFFLINE);
}

bool Desktop::isCurrent(const QWidget *widget) const {
    return ui.pagerStack->currentWidget() == widget;
}

void Desktop::initial()
{
    Credentials = login->credentials();
    if(Credentials.isEmpty())
        return;

    listen(Credentials);
    ui.toolBar->show();
    showSessions();         // FIXME: this is for early testing only....
    authorizing();
}

void Desktop::authorizing()
{
    ui.trayAway->setText(tr("..."));
    ui.trayAway->setEnabled(false);
    status(tr("Authorizing..."));
    setState(AUTHORIZING);
}

void Desktop::authorized(const QVariantHash& creds)
{
    Credentials = creds;
    Credentials["initialize"] = ""; // only exists for signin...

    // apply or update credentials only after successfull authorization
    if(storage)
        storage->updateCredentials(creds);
    else
        storage = new Storage("", creds);

    // this is how we should get out of first time login screen...
    if(isLogin())
        showSessions();

    if(!connected) {
        ui.trayAway->setText(tr("Away"));
        ui.trayAway->setEnabled(true);
        connected = true;
        emit online(connected);
    }
    setState(ONLINE);
}

void Desktop::listen(const QVariantHash& cred)
{
    Q_ASSERT(listener == nullptr);

    listener = new Listener(cred);
    connect(listener, &Listener::authorize, this, &Desktop::authorized);
    connect(listener, &Listener::starting, this, &Desktop::authorizing);
    connect(listener, &Listener::finished, this, &Desktop::offline);
    connect(listener, &Listener::failure, this, &Desktop::failed);
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
