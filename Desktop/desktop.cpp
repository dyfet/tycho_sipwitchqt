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
#include "../Dialogs/about.hpp"
#include "../Dialogs/resetdb.hpp"
#include "desktop.hpp"
#include "ui_desktop.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <QAbstractNativeEventFilter>
#endif

#include <QTranslator>
#include <QFile>
#include <QCloseEvent>
#include <QResizeEvent>
#include <csignal>

static int result = 0;

static void signal_handler(int signo)
{
    Desktop *desktop = Desktop::instance();
    if(desktop) {
        QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
    }

    result = signo;
}

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
    QMetaObject::invokeMethod(desktop, "dockClicked", Qt::QueuedConnection);
    return false;
}
#endif

#ifdef Q_OS_WIN

class NativeEvent final : public QAbstractNativeEventFilter
{
private:
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) final;
};

bool NativeEvent::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);

    MSG *msg = static_cast<MSG*>(message);
    Desktop *desktop = nullptr;

    Q_UNUSED(desktop);

    switch(msg->message) {
    case WM_POWERBROADCAST:
        switch(msg->wParam) {
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMRESUMESUSPEND:
//          resume...
            break;
        case PBT_APMSUSPEND:
//          suspend..
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return false;
}

#endif

static Ui::MainWindow ui;

Desktop *Desktop::Instance = nullptr;
Desktop::state_t Desktop::State = Desktop::INITIAL;
QVariantHash Desktop::Credentials;

Desktop::Desktop(bool tray, bool reset) :
QMainWindow(), listener(nullptr), storage(nullptr), settings(CONFIG_FROM), dialog(nullptr)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;
    connector = nullptr;
    front = true;

#ifdef Q_OS_WIN
    static NativeEvent nativeEvents;
    qApp->installNativeEventFilter(&nativeEvents);
#endif

    if(reset) {
        Storage::remove();
        settings.clear();
    }

    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setIconSize(QSize(16, 16));

    currentExpiration = 86400 * settings.value("expires", 7).toInt();
    currentAppearance = settings.value("appearance", "Vibrant").toString();

    control = new Control(this);

    QString prefix = ":/icons/";
    if(currentAppearance == "Vibrant")
        prefix += "color_";

    ui.setupUi(static_cast<QMainWindow *>(this));
    toolbar = new Toolbar(this, ui.toolBar);
    statusbar = new Statusbar(ui.centralwidget, ui.statusBar);

    appBar = nullptr;
    trayIcon = nullptr;
    trayMenu = dockMenu = appMenu = popup = nullptr;

    connect(ui.appQuit, &QAction::triggered, qApp, &QApplication::quit);
    connect(ui.appAbout, &QAction::triggered, this, &Desktop::openAbout);
    connect(ui.appPreferences, &QAction::triggered, this, &Desktop::showOptions);

#if defined(Q_OS_MAC)
    appBar = new QMenuBar(this);
    appMenu = appBar->addMenu(tr("SipWitchQt Desktop"));
    appMenu->addAction(ui.appAbout);
    appMenu->addAction(ui.appPreferences);
    appMenu->addAction(ui.appQuit);
    appBar->show();

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
                set_dock_icon(QIcon(prefix + "offline.png"));
                tray = false;
            }
        }
        else {
            if (class_addMethod(handler, shouldHandle, reinterpret_cast<IMP>(dock_click_handler), "B@:")) {
                tray = false;
                set_dock_icon(QIcon(prefix + "offline.png"));
            }
        }
    }

    if(tray)
        set_dock_icon(QIcon(prefix + "idle.png"));

#endif
    login = new Login(this);
    sessions = new Sessions(this);
    options = new Options(this);
    phonebook = new Phonebook(this, sessions);
    ui.pagerStack->addWidget(login);
    ui.pagerStack->addWidget(sessions);
    ui.pagerStack->addWidget(phonebook);
    ui.pagerStack->addWidget(options);
    ui.console->setHidden(true);

    connect(ui.actionMenu, &QAction::triggered, this, &Desktop::menuClicked);
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
        trayIcon->setIcon(QIcon(prefix + "offline.png"));
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

    setWindowTitle("Welcome to SipWitchQt " PROJECT_VERSION);
    if(Storage::exists()) {
        storage = new Storage("");
        emit changeStorage(storage);
        showSessions();
        restoreGeometry(settings.value("geometry").toByteArray());
        show();                             // FIXME: hide
        listen(storage->credentials());
    }
    else {
        ui.trayAway->setEnabled(false);
        warningMessage(tr("No local database active"));
        ui.toolBar->hide();
        ui.pagerStack->setCurrentWidget(login);
        login->enter();
        show();
        settings.setValue("geometry", saveGeometry());
        ui.appPreferences->setEnabled(false);
    }
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

QMenu *Desktop::createPopupMenu()
{
    clearMessage();
    return nullptr;
}

void Desktop::shutdown()
{   
    bool threads = false;

    if(connector) {
        threads = true;
        connector->stop(true);
        connector = nullptr;
    }

    if(listener) {
        threads = true;
        listener->stop(true);
        listener = nullptr;
    }

    if(storage) {
        emit changeStorage(nullptr);
        delete storage;
        storage = nullptr;
    }

    if(control) {
        delete control;
        control = nullptr;
    }

    if(threads) {
        qDebug() << "Shutting down threads...";
        QThread::msleep(500);       // sleep long enough for stack to end...
    }
}

void Desktop::setState(state_t state)
{
    // FIXME: Eventually adjusted by appearance
    QString prefix = ":/icons/";
    QString icon;

    State = state;

    if(currentAppearance == "Vibrant")
        prefix += "color_";

    switch(State) {
    case AUTHORIZING:
        icon = prefix + "activate.png";
        break;
    case ONLINE:
        icon = prefix + "idle.png";
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

void Desktop::openAbout()
{
    closeDialog();
    setEnabled(false);
    dialog = new About(this);
}

void Desktop::openReset()
{
    closeDialog();
    setEnabled(false);
    dialog = new ResetDb(this);
}

void Desktop::closeReset()
{
    Q_ASSERT(dialog != nullptr);
    Q_ASSERT(storage != nullptr);

    emit changeStorage(nullptr);
    closeDialog();
    warningMessage(tr("Resetting database..."));
    offline();
    delete storage;
    storage = nullptr;
    ui.toolBar->hide();
    ui.pagerStack->setCurrentWidget(login);
    login->enter();
    ui.appPreferences->setEnabled(false);
}

void Desktop::closeDialog()
{
    if(dialog) {
        dialog->deleteLater();
        setEnabled(true);
        dialog = nullptr;
    }
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

void Desktop::warningMessage(const QString& text, int timeout)
{
    ui.statusBar->setStyleSheet("color: orange;");
    ui.statusBar->showMessage(text, timeout);
}

void Desktop::errorMessage(const QString& text, int timeout)
{
    ui.statusBar->setStyleSheet("color: red;");
    ui.statusBar->showMessage(text, timeout);
}

void Desktop::statusMessage(const QString& text, int timeout)
{
    ui.statusBar->setStyleSheet("color: blue;");
    ui.statusBar->showMessage(text, timeout);
}

void Desktop::clearMessage()
{
    ui.statusBar->clearMessage();
}

void Desktop::changeExpiration(const QString& expires)
{
    settings.setValue("expires", expires.toInt());
    currentExpiration = (86400 * expires.toInt());
}

void Desktop::changeAppearance(const QString& appearance)
{
    currentAppearance = appearance;
    settings.setValue("appearance", appearance);
    setState(State);
}

void Desktop::appState(Qt::ApplicationState state)
{
    // Actve, Inactive related to front/back...
    switch(state) {
    case Qt::ApplicationActive:
        QTimer::singleShot(200, this, [=] {
            front = true;
        });
        break;
    case Qt::ApplicationHidden:
    case Qt::ApplicationInactive:
        front = false;
        break;
    default:
        break;
    }

    qDebug() << "Application State" << state;
}

void Desktop::menuClicked()
{
    if(popup) {
        ui.actionMenu->setMenu(nullptr);
        setEnabled(true);
        popup->deleteLater();
        popup = nullptr;
        return;
    }

    setEnabled(false);
    popup = new QMenu(this);
    popup->addAction(ui.appAbout);
    if(storage && (State == Desktop::OFFLINE || State == Desktop::ONLINE))
        popup->addAction(ui.trayAway);
    popup->addAction(ui.appQuit);
    popup->popup(this->mapToGlobal(QPoint(4, 24)));
    popup->show();
    popup->setEnabled(true);

    ui.actionMenu->setMenu(popup);
    connect(popup, &QMenu::aboutToHide, this, &Desktop::menuClicked);
}

void Desktop::dockClicked()
{
    trayAction(QSystemTrayIcon::MiddleClick);
}

void Desktop::trayAway()
{
    if(connector) {
        statusMessage(tr("disconnect"));
        offline();
    }
    else if(storage)
        listen(storage->credentials());
}

void Desktop::trayAction(QSystemTrayIcon::ActivationReason reason)
{
    time_t now;
    time(&now);
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
        else if(front)
            hide();
        break;
    default:
        break;
    }
}

void Desktop::failed(int error_code)
{
    offline();
    switch(error_code) {
    case SIP_CONFLICT:
        errorMessage(tr("Label already used"));
        if(isLogin())
            login->badLabel();
        break;
    case SIP_DOES_NOT_EXIST_ANYWHERE:
        errorMessage(tr("Extension number is invalid"));
        if(isLogin())
            login->badIdentity();
        break;
    case SIP_NOT_FOUND:
        errorMessage(tr("Extension not found"));
        if(isLogin())
            login->badIdentity();
        break;
    case SIP_FORBIDDEN:
    case SIP_UNAUTHORIZED:
        errorMessage(tr("Authorizartion denied"));
        if(isLogin())
            login->badPassword();
        break;
    case SIP_METHOD_NOT_ALLOWED:
        errorMessage(tr("Registration not allowed"));
        if(isLogin())
            login->badIdentity();
        break;
    case SIP_INTERNAL_SERVER_ERROR:
    case 666:
        errorMessage(tr("Cannot reach server"));
        if(isLogin())
            login->badIdentity();
        break;
    default:
        errorMessage(tr("Unknown server failure"));
        if(isLogin())
            login->badIdentity();
        break;
    }
}

void Desktop::offline()
{
    // if already offline, we can ignore...
    if(!listener)
        return;

    // could return from authorizing to offline...
    ui.trayAway->setText(tr("Connect"));
    ui.trayAway->setEnabled(!isLogin());

    if(connector) {
        connector->stop();
        connector = nullptr;
        emit changeConnector(nullptr);
    }
    listener->stop();
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

    Storage::remove();      // remove before trying an initial login...
    ui.toolBar->show();
    listen(Credentials);
}

void Desktop::authorizing()
{
    ui.trayAway->setText(tr("..."));
    ui.trayAway->setEnabled(false);
    statusMessage(tr("Authorizing..."));
    setState(AUTHORIZING);
}

void Desktop::authorized(const QVariantHash& creds)
{
    Credentials = creds;
    Credentials["initialize"] = ""; // only exists for signin...

    // apply or update credentials only after successfull authorization
    if(storage)
        storage->updateCredentials(creds);
    else {
        auto server = creds["host"].toString().toUtf8();
        auto schema = creds["schema"].toString().toUtf8();
        auto uri = QString(UString::uri(schema, creds["extension"].toString(), server, static_cast<quint16>(creds["port"].toInt())));
        auto opr = QString(UString::uri(schema, "0", server, static_cast<quint16>(creds["port"].toInt())));
        storage = new Storage("", creds);
        storage->runQuery("INSERT INTO Contacts(extension, dialing, display, user, uri) "
                          "VALUES(?,?,?,?,?);", {
                              creds["extension"],
                              creds["extension"].toString(),
                              creds["display"],
                              creds["user"],
                              uri,
                          });
        storage->runQuery("INSERT INTO Contacts(extension, dialing, display, user, type, uri) "
                          "VALUES(0,'0', 'Operators', 'operators', 'SYSTEM',?);", {opr});
        emit changeStorage(storage);
    }

    // this is how we should get out of first time login screen...
    if(isLogin())
        showSessions();

    if(!connector) {
        ui.trayAway->setText(tr("Away"));
        ui.trayAway->setEnabled(true);
        ui.appPreferences->setEnabled(true);
        connector = new Connector(creds);
        connector->start();
        emit changeConnector(connector);
        statusMessage(tr("online"));
    }
    setState(ONLINE);
}

void Desktop::setBanner(const QString& banner)
{
    setWindowTitle(banner);
}

void Desktop::listen(const QVariantHash& cred)
{
    Q_ASSERT(listener == nullptr);

    listener = new Listener(cred);
    connect(listener, &Listener::authorize, this, &Desktop::authorized);
    connect(listener, &Listener::starting, this, &Desktop::authorizing);
    connect(listener, &Listener::finished, this, &Desktop::offline);
    connect(listener, &Listener::failure, this, &Desktop::failed);
    connect(listener, &Listener::changeBanner, this, &Desktop::setBanner);
    listener->start();
    authorizing();
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

#ifdef SIGHUP
    ::signal(SIGHUP, signal_handler);
#endif
#ifdef SIGKILL
    ::signal(SIGKILL, signal_handler);
#endif
    ::signal(SIGINT, signal_handler);
    ::signal(SIGTERM, signal_handler);

    args.process(app);
    Desktop w(!args.isSet("notray"), args.isSet("reset"));
    int status = app.exec();
    if(!result)
        result = status;
    return result;
}
