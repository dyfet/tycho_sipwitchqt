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
#include "../Common/crypto.hpp"
#include "../Common/args.hpp"
#include "../Dialogs/about.hpp"
#include "../Dialogs/devicelist.hpp"
#include "../Dialogs/logout.hpp"
#include "../Dialogs/adduser.hpp"
#include "../Dialogs/newgroup.hpp"
#include "../Dialogs/delauth.hpp"
#include "desktop.hpp"
#include "ui_desktop.h"
#include <QCryptographicHash>
#include <QJsonObject>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <QAbstractNativeEventFilter>
#ifndef QT_NO_DEBUG_OUTPUT
#define STARTUP_MINIMIZED
#endif
#endif

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#ifdef Q_OS_MAC
#include <objc/objc.h>
#include <objc/message.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

void send_notification(const UString& text, const UString& info);
void set_dock_icon(const QIcon& icon);
void set_dock_label(const UString& text);
void disable_nap();
#endif

#include <QGuiApplication>
#include <QTranslator>
#include <QFile>
#include <QCloseEvent>
#include <QResizeEvent>
#include <csignal>
#include <QFileDialog>
#include <QMessageBox>
#include <QMutex>

namespace {
int result = 0;

void signal_handler(int signo) {
    Desktop *desktop = Desktop::instance();
    if (desktop) {
        QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection); // NOLINT
    }

    result = signo;
}

#ifdef Q_OS_MAC
io_connect_t ioroot = 0;
io_object_t iopobj;
IONotificationPortRef iopref;

bool dock_click_handler(::id self, SEL _cmd, ...) {
    Q_UNUSED(self)
    Q_UNUSED(_cmd)

    auto desktop = Desktop::instance();
    QMetaObject::invokeMethod(desktop, "dockClicked", Qt::QueuedConnection);
    return false;
}

void iop_callback(void *ref, io_service_t ioservice, natural_t mtype, void *args) {
    Q_UNUSED(ioservice);
    Q_UNUSED(ref);

    auto desktop = Desktop::instance();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"

    switch (mtype) {
        case kIOMessageCanSystemSleep:
            IOAllowPowerChange(ioroot, (long) args); // NOLINT
            break;
        case kIOMessageSystemWillSleep:
            QMetaObject::invokeMethod(desktop, "powerSuspend", Qt::QueuedConnection);
            IOAllowPowerChange(ioroot, (long) args); // NOLINT
            break;
        case kIOMessageSystemHasPoweredOn:
            QMetaObject::invokeMethod(desktop, "powerResume", Qt::QueuedConnection);
            break;
        default:
            break;
    }

#pragma clang pop
}

void iop_startup() {
    void *ref = nullptr;

    if (ioroot != 0)
        return;

    ioroot = IORegisterForSystemPower(ref, &iopref, iop_callback, &iopobj);
    if (!ioroot) {
        qWarning() << "Registration for power management failed";
        return;
    } else
        qDebug() << "Power management enabled";

    CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(iopref), kCFRunLoopCommonModes);
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
    auto desktop = Desktop::instance();

    switch(msg->message) {
    case WM_POWERBROADCAST:
        switch(msg->wParam) {
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMRESUMESUSPEND:
            QMetaObject::invokeMethod(desktop, "powerResume", Qt::QueuedConnection);
            break;
        case PBT_APMSUSPEND:
            QMetaObject::invokeMethod(desktop, "powerSuspend", Qt::QueuedConnection);
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

Ui::MainWindow ui;
} // namespace

Desktop *Desktop::Instance = nullptr;
Desktop::state_t Desktop::State = Desktop::INITIAL;
QVariantHash Desktop::Credentials;

Desktop::Desktop(const QCommandLineParser& args) :
listener(nullptr), storage(nullptr), settings(CONFIG_FROM), dialog(nullptr)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;
    connector = nullptr;
    front = true;
    powerReconnect = false;
    updateRoster = true;
    offlineMode = true;

    // for now, just this...
    baseFont = getBasicFont();
    baseHeight = QFontInfo(baseFont).pixelSize();

#ifdef Q_OS_WIN
    static NativeEvent nativeEvents;
    qApp->installNativeEventFilter(&nativeEvents);
#endif

    bool tray = !args.isSet("notray");
    if(args.isSet("reset")) {
        Storage::remove(dbName);
        settings.clear();
    }

    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setIconSize(QSize(16, 16));

    // Settings have to be extracted from config before options ui is created
    currentExpiration = settings.value("expires", 7 * 86400).toInt();
    currentAppearance = settings.value("appearance", "Vibrant").toString();
    autoAnswer = settings.value("autoanswer", false).toBool();
    desktopNotify = settings.value("notify-desktop", true).toBool();
    audioNotify = settings.value("notify-audio", true).toBool();

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

    connect(ui.appLogout, &QAction::triggered, this, &Desktop::openLogout);
    connect(ui.appPreferences, &QAction::triggered, this, &Desktop::showOptions);

#if defined(Q_OS_MAC)
    iop_startup();

    appBar = new QMenuBar(this);
    appMenu = appBar->addMenu(tr("SipWitchQt Desktop"));
    appMenu->addAction(ui.appAbout);
    appMenu->addAction(ui.appPreferences);
    appMenu->addAction(ui.appQuit);
    appBar->show();

    dockMenu = new QMenu();
    dockMenu->addAction(ui.trayDnd);
    dockMenu->addAction(ui.trayAway);
    dockMenu->addAction(ui.trayLogout);
    qt_mac_set_dock_menu(dockMenu);

    auto inst = objc_msgSend(reinterpret_cast<objc_object *>(objc_getClass("NSApplication")), sel_registerName("sharedApplication")); // NOLINT

    if(inst) {
        auto delegate = objc_msgSend(inst, sel_registerName("delegate")); // NOLINT
        auto handler = reinterpret_cast<Class>(objc_msgSend(delegate, sel_registerName("class"))); // NOLINT

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
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Desktop::shutdown); // NOLINT
    connect(qApp, &QGuiApplication::applicationStateChanged, this, &Desktop::appState); // NOLINT
    connect(phonebook, &Phonebook::removeSelf, this, &Desktop::eraseLogout, Qt::QueuedConnection);

    connect(sessions, &Sessions::changeWidth, [this](int width) {
        settings.setValue("width", width);
        phonebook->setWidth(width);
    });

    connect(phonebook, &Phonebook::changeWidth, [this](int width) {
        settings.setValue("width", width);
        sessions->setWidth(width);
    });

    connect(&dailyTimer, &QTimer::timeout, [this]{
        qDebug() << "Daily housekeeping activated";
        if(storage)
            storage->cleanupExpired();
        emit dailyEvent();
        dailyTimer.setSingleShot(true);
        dailyTimer.start(Util::untilInterval(Util::DAILY_INTERVAL));
    });

    if(tray)
        trayIcon = new QSystemTrayIcon(this);

    if(trayIcon) {
        trayMenu = new QMenu();
        trayMenu->addAction(ui.trayDnd);
        trayMenu->addAction(ui.trayAway);
        trayMenu->addAction(ui.trayLogout);
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
    connect(ui.trayLogout, &QAction::triggered, this, &Desktop::openLogout);

    login = new Login(this);
    ui.pagerStack->addWidget(login);

    setWindowTitle("Welcome to SipWitchQt " PROJECT_VERSION);
    dbName = settings.value("database").toString();
    if(Storage::exists(dbName)) {
        storage = new Storage(dbName, "");
        if(storage && !storage->isActive()) {
            errorMessage(tr("*** Database in use ***"));
            delete storage;
            storage = nullptr;
        }
    }
    if(storage) {
        emit changeStorage(storage);
        showSessions();

#ifndef STARTUP_MINIMIZED
        if(args.isSet("minimize"))
            hide();
        else {
            show();
            resize(settings.value("size", size()).toSize());
            move(settings.value("position", pos()).toPoint());
        }
#else
        hide();
#endif

        listen(storage->credentials());
    }
    else {
        ui.trayAway->setEnabled(false);
        warningMessage(tr("No local database active"));
        ui.toolBar->hide();
        ui.pagerStack->setCurrentWidget(login);
        login->enter();
        show();
        settings.setValue("size", size());
        settings.setValue("position", pos());
        ui.appPreferences->setEnabled(false);
    }
    auto width = settings.value("width").toInt();
    if(!width)
        width = 136;
    sessions->setWidth(width);
    phonebook->setWidth(width);

    // enable timer initialization...
    dailyTimer.setSingleShot(true);
    dailyTimer.start(Util::untilInterval(Util::DAILY_INTERVAL));
}

void Desktop::closeEvent(QCloseEvent *event)
{
    if(isVisible()) {
        settings.setValue("size", size());
        settings.setValue("position", pos());
    }

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

    if(isVisible()) {
        settings.setValue("size", size());
        settings.setValue("position", pos());
    }

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

void Desktop::setUnread(unsigned count)
{
#ifdef Q_OS_MAC
    UString text = UString::number(count);
    if(!count)
        text = "";
    set_dock_label(text);
#else
    Q_UNUSED(count); // TODO: gnome and windows
#endif
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

void Desktop::openDelAuth(const UString& id)
{
    closeDialog();
    setEnabled(false);
    auto obj = new DelAuth(this, connector, id);
    dialog = obj;
    connect(obj, &DelAuth::error, this, [this](const QString& msg) {
        errorMessage(msg);
    });
}

void Desktop::openNewGroup()
{
    closeDialog();
    setEnabled(false);
    auto obj = new NewGroup(this, connector);
    dialog = obj;
    connect(obj, &NewGroup::error, this, [this](const QString& msg) {
        errorMessage(msg);
    });
}

void Desktop::openAddUser()
{
    closeDialog();
    setEnabled(false);
    auto obj = new AddUser(this, connector);
    dialog = obj;
    connect(obj, &AddUser::error, this, [this](const QString& msg) {
        errorMessage(msg);
    });
}

void Desktop::openAbout()
{
    closeDialog();
    setEnabled(false);
    dialog = new About(this);
}

void Desktop::openLogout()
{
    closeDialog();
    setEnabled(false);
    dialog = new Logout(this);
}

void Desktop::eraseLogout()
{
    Q_ASSERT(dialog != nullptr);
    Q_ASSERT(storage != nullptr);

    emit logout();

    offlineMode = true;
    settings.setValue("database", "");
    emit changeStorage(nullptr);
    closeDialog();
    warningMessage(tr("logging out and erasing database..."));
    offline();
    delete storage;
    storage = nullptr;
    Storage::remove(dbName);
    ui.toolBar->hide();
    ui.pagerStack->setCurrentWidget(login);
    login->enter();
    ui.appPreferences->setEnabled(false);
    updateRoster = true;
}

void Desktop::closeLogout()
{
    Q_ASSERT(dialog != nullptr);
    Q_ASSERT(storage != nullptr);

    emit logout();

    offlineMode = true;
    settings.setValue("database", "");
    emit changeStorage(nullptr);
    closeDialog();
    warningMessage(tr("logging out..."));
    offline();
    delete storage;
    storage = nullptr;
    ui.toolBar->hide();
    ui.pagerStack->setCurrentWidget(login);
    login->enter();
    ui.appPreferences->setEnabled(false);
    updateRoster = true;        // force update at next login...
}

void Desktop::closeDialog()
{
    if(dialog) {
        dialog->deleteLater();
        setEnabled(true);
        dialog = nullptr;
    }
}

void Desktop::openDeviceList()
{
    if(!connector) {
        errorMessage(tr("Must be online to get device list"));
        return;
    }
    closeDialog();
    setEnabled(false);
    dialog = new DeviceList(this, connector);
}

void Desktop::showOptions()
{
    ui.pagerStack->setCurrentWidget(options);
    toolbar->noSearch();
    toolbar->noSession();
    options->enter();
}

void Desktop::showSessions()
{
    ui.pagerStack->setCurrentWidget(sessions);
    toolbar->showSearch();
    toolbar->noSession();
    sessions->enter();
}

void Desktop::showPhonebook()
{
    ui.pagerStack->setCurrentWidget(phonebook);
    toolbar->showSearch();
    toolbar->noSession();
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

void Desktop::changeExpiration(int expires)
{

    currentExpiration = expires;
    settings.setValue("expires", expires);
    if(storage) {
        auto lastRecord = storage->getRecord("Select count(*) from Messages;");
        qDebug() << "Last record value is " << lastRecord;
        auto query = storage->getRecords("SELECT posted, rowid FROM Messages");
        while(query.isActive() && query.next()) {
            auto record = query.record();
            auto posted = record.value("posted").toDateTime();
            auto erase = posted.addSecs(expires);
            auto id = record.value("rowid").toLongLong();

            auto result = storage->runQuery("UPDATE Messages set expires=? where rowid=?;", {erase, id});
            if (result)
            {
//                qDebug() << "Posted is " << posted << " changed expires to " << erase << " for message with id " << id << endl ;
            }
           else {
               qDebug() << "Failed to update the record check your source code" << endl;
           }
        }
        sessions->clearSessions();
    }
    qDebug() << "Expiration changed to " << currentExpiration << " in seconds." << endl;
}

void Desktop::changePassword(const QString& password)
{
    if(password == Credentials["secret"]) {
        errorMessage(tr("Password unchanged"));
        return;
    }

    if(!connector) {
        errorMessage(tr("Cannot change password offline"));
        return;
    }

    auto realm = Credentials["realm"].toString();
    auto user = Credentials["user"].toString();
    auto digest = Credentials["algorithm"].toString().toUpper();
    qDebug() << "Change password for" << user << "," << realm << digest;
    auto ha1 = user + ":" + realm + ":" + password;
    auto secret = QCryptographicHash::hash(ha1.toUtf8(), Crypto::digests[digest]).toHex();
    qDebug() << "Secret" << secret;

    QJsonObject json = {
        {"s", QString::fromUtf8(secret)},
        {"n", Phonebook::self()->number()},
    };
    QJsonDocument jdoc(json);
    auto body = jdoc.toJson(QJsonDocument::Compact);

    connector->sendProfile(Phonebook::self()->uri(), body);
    showSessions();
    setEnabled(false);
    offlineMode = true;
    offline();
    Credentials["secret"] = password;
    statusMessage(tr("Changing password..."), 2500);

    QTimer::singleShot(2400, this, [this]{
        if(!connector && !listener)
            listen(Credentials);
        setEnabled(true);
    });
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
        if(storage != nullptr && listener == nullptr && !front && !offlineMode) {
            qDebug() << "Going online on activation";
            listen(storage->credentials());
        }
        QTimer::singleShot(200, this, [this] {
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

//    qDebug() << "Application State" << state;             // disabled because it is always openning application output every few second
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
    popup->addAction(ui.trayDnd);
    popup->addAction(ui.appLogout);
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

void Desktop::powerSuspend()
{
    qDebug() << "*** POWER SUSPEND";
    if(connector) {
        closeDialog();
        setEnabled(false);
        statusMessage(tr("suspending..."));
        offlineMode = true;
        offline();
        powerReconnect = true;
    }
    else
        powerReconnect = false;
}

void Desktop::powerResume()
{
    qDebug() << "*** POWER RESUME";
    if(!connector && powerReconnect && storage) {
        powerReconnect = false;
        statusMessage(tr("resuming..."));
        QTimer::singleShot(1200, this, [this]() {
            setEnabled(true);
            if(!connector && !listener) {
                listen(storage->credentials());
            }
        });
    }
    else
        setEnabled(true);
}

void Desktop::trayAway()
{
    if(connector) {
        offlineMode = true;
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

void Desktop::statusResult(int status, const QString& text)
{
    QString msg;

    switch(status) {
    case SIP_OK:
        if(text.isEmpty())
            clearMessage();
        else
            statusMessage(text);
        return;
    case SIP_NOT_FOUND:
    case SIP_DOES_NOT_EXIST_ANYWHERE:
        msg = tr("Unknown extension or name used");
        break;
    case SIP_CONFLICT:
        msg = tr("Cannot modify existing");
        break;
    case SIP_FORBIDDEN:
    case SIP_UNAUTHORIZED:
    case SIP_METHOD_NOT_ALLOWED:
        msg = tr("Unauthorized or invalid request");
        break;
    case SIP_INTERNAL_SERVER_ERROR:
        msg = tr("Server request failed");
        break;
    case 666:
        failed(666);
        return;
    default:
        msg = tr("Request failed or rejected");
        break;
    }

    if(!text.isEmpty())
        msg = text;

    if(!msg.isEmpty())
        errorMessage(msg);
}

void Desktop::failed(int error_code)
{
    offline();
    switch(error_code) {
    case SIP_CONFLICT:
        FOR_DEBUG(
            qDebug() << "Label already used: SIP_CONFLICT";
            errorMessage(tr("Label already used ") + "SIP_CONFLICT");
        )
        FOR_RELEASE(
            errorMessage(tr("Label already used"));
        )
        if(isLogin())
            login->badLabel();
        break;
    case SIP_DOES_NOT_EXIST_ANYWHERE:
        FOR_DEBUG(
            qDebug() << "Extension number is invalid: SIP_DOES_NOT_EXIST_ANYWHERE";
            errorMessage(tr("Extension number is invalid ") + "SIP_DOES_NOT_EXIST_ANYWHERE");
        )
        FOR_RELEASE(
            errorMessage(tr("Extension number is invalid"));
        )
        if(isLogin())
            login->badIdentity();
        break;
    case SIP_NOT_FOUND:
        FOR_DEBUG(
            qDebug() << "Extension not found: SIP_NOT_FOUND";
            errorMessage(tr("Extension not found ") + "SIP_NOT_FOUND");
        )
        FOR_RELEASE(
            errorMessage(tr("Extension not found"));
        )
        if(isLogin())
            login->badIdentity();
        break;
    case SIP_FORBIDDEN:
    case SIP_UNAUTHORIZED:
        FOR_DEBUG(
            qDebug() << "Authorizartion denied: SIP_UNAUTHORIZED";
            errorMessage(tr("Authorizartion denied ") + "SIP_UNAUTHORIZED");
        )
        FOR_RELEASE(
            errorMessage(tr("Authorizartion denied"));
        )
        if(isLogin())
            login->badPassword();
        break;
    case SIP_METHOD_NOT_ALLOWED:
        FOR_DEBUG(
            qDebug() << "Registration not allowed: SIP_METHOD_NOT_ALLOWED";
            errorMessage(tr("Registration not allowed ") + "SIP_METHOD_NOT_ALLOWED");
        )
        FOR_RELEASE(
            errorMessage(tr("Registration not allowed"));
        )
        if(isLogin())
            login->badIdentity();
        break;
    case SIP_INTERNAL_SERVER_ERROR:
    case 666:
        FOR_DEBUG(
            qDebug() << "Cannot reach server: SIP_INTERNAL_SERVER_ERROR" << error_code << QDateTime::currentDateTime();
            errorMessage(tr("Cannot reach server ") + "SIP_INTERNAL_SERVER_ERROR");
        )
        FOR_RELEASE(
            errorMessage(tr("Cannot reach server"));
        )
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

    qDebug() << "Going offline";
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

    auto dbName = "sipwitchqt-" + Storage::name(Credentials, "desktop");
    Storage::initialLogin(dbName, "", Credentials);

    login->setEnabled(false);
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
    offlineMode = false;

    // apply or update credentials only after successfull authorization
    if(storage)
        storage->updateCredentials(creds);
    else {
        auto server = creds["host"].toString().toUtf8();
        auto schema = creds["schema"].toString().toUtf8();
        auto uri = QString(UString::uri(schema, creds["extension"].toString(), server, static_cast<quint16>(creds["port"].toInt())));
        auto opr = QString(UString::uri(schema, "0", server, static_cast<quint16>(creds["port"].toInt())));
        dbName = "sipwitchqt-" + Storage::name(creds, "desktop");
        settings.setValue("database", dbName);
        storage = new Storage(dbName, "", creds);
        if(!storage->isActive()) {
            errorMessage(tr("*** Database failed to create ***"));
            storage = nullptr;
            return;
        }
        if(!storage->isExisting()) {
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
        }
        emit changeStorage(storage);
    }

    ui.toolBar->show();
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
        connect(connector, &Connector::failure, this, &Desktop::failed);
        connect(connector, &Connector::statusResult, this, &Desktop::statusResult);
        connect(connector, &Connector::deauthorizeUser, this, &Desktop::removeUser);
    }
    setState(ONLINE);
}

void Desktop::listen(const QVariantHash& cred)
{
    Q_ASSERT(listener == nullptr);
    peeringAddress = "unknown";

    listener = new Listener(cred);
    connect(listener, &Listener::authorize, this, &Desktop::authorized);
    connect(listener, &Listener::starting, this, &Desktop::authorizing);
    connect(listener, &Listener::finished, this, &Desktop::offline);
    connect(listener, &Listener::failure, this, &Desktop::failed);
    connect(listener, &Listener::statusResult, this, &Desktop::statusResult);

    connect(listener, &Listener::changeBanner, this, [this](const QString& banner) {
        setWindowTitle(banner);
    }, Qt::QueuedConnection);

    connect(listener, &Listener::contactAddress, this, [this](const QString& address) {
        peeringAddress = address;
    }, Qt::QueuedConnection);

    connect(listener, &Listener::updateRoster, this, [this]() {
        if(!updateRoster && connector)
            connector->requestRoster();
        updateRoster = true;
    }, Qt::QueuedConnection);

    sessions->listen(listener);
    listener->start();
    emit changeListener(listener);
    authorizing();
}

void Desktop::setSelf(const QString& text)
{
    if(storage) {
        storage->updateSelf(text);
        emit changeSelf(text);
    }
}

// Calls the Storage::copyDb function to copy existing database
void Desktop::exportDb()
{
    if(!storage) {
        QMessageBox::warning(this, tr("No active database"),tr("Database cannot be backed up"));
    }
    else if(storage->copyDb(dbName) != 0)
    {
        QMessageBox::warning(this, tr("Unable to backup the database"),tr("Database cannot be backed up backup file already exist."));
    }
    else
    {
        auto ext = credentials()["extension"].toString();
        auto lab = credentials()["label"].toString();
        QString filename = ext + "-" + lab + "-" + "backup.db";
        QMessageBox::information(this, tr("Database succesfully backed up"),tr("Database saved under name ") + filename);

    }
}

// Import database after make sure that client has went offline.
void Desktop::importDb()
{
    Q_ASSERT(storage != nullptr);

    emit changeStorage(nullptr);
    warningMessage(tr("logging out..."));
    offlineMode = true;
    offline();
    delete storage;
    storage = nullptr;

    auto ext = credentials()["extension"].toString();
    auto lab = credentials()["label"].toString();
    QString filename = ext + "-" + lab + "-" + "backup.db";
    ui.toolBar->hide();
    ui.pagerStack->setCurrentWidget(login);
    login->enter();
    ui.appPreferences->setEnabled(false);

    if(!Storage::importDb(dbName, Credentials)) {
        QMessageBox::warning(this, tr("Unable to import database"),tr("Database backup do not exist"));
    }
    else {
        QMessageBox::information(this, tr("Database succesfully restored"),tr("Database imported from the file") + filename);
        storage = new Storage(dbName, "");
        if(storage && !storage->isActive()) {
            errorMessage(tr("*** Restore failed ***"));
            delete storage;
            storage = nullptr;
        }
        else {
            emit changeStorage(storage);
            listen(storage->credentials());
        }
    }

}

void Desktop::removeUser(const QString &id)
{
    // kickback function to remove after de-authorization accepted by server

    if(!storage)
        return;

    qDebug() << "ID!" << id;

    auto list = ContactItem::findAuth(id);
    if(list.count() < 1) {
        errorMessage(tr("No users to remove"));
        return;
    }

    warningMessage(tr("Removing ") + QString::number(list.count()) + tr(" extensions..."));
    qDebug() << "Removing" << id << "for" << list.count();

    foreach(auto item, list) {
        // make sure gone from database before possible crash...
        // if crashed, will pick up changes from the roster sync.

        storage->runQuery("DELETE FROM Messages WHERE sid=?;", {item->id()});
        storage->runQuery("DELETE FROM Contacts WHERE uid=?;", {item->id()});

        sessions->remove(item);
        phonebook->remove(item);
    }
    ContactItem::deauthorize(id);
}

#ifdef Q_OS_MAC
void Desktop::notification(const QString& title, const QString& info)
{
    if(desktopNotify)
        send_notification(title.toUtf8(), info.toUtf8());
}
#else
void Desktop::notification(const QString& title, const QString& info)
{
    if(trayIcon && desktopNotify)
        trayIcon->showMessage(title, info);
}
#endif

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationVersion(PROJECT_VERSION);
    QCoreApplication::setOrganizationName(APPLICATION_ORG);
    QCoreApplication::setOrganizationDomain(APPLICATION_DOMAIN);
    QCoreApplication::setApplicationName(APPLICATION_NAME);
    QTranslator localize;

    QApplication app(argc, argv);
    QCommandLineParser args;
    Q_INIT_RESOURCE(desktop);

#ifdef CUSTOMIZE_DESKTOP
    Q_INIT_RESOURCE(custom);
#endif

#if defined(DEBUG_TRANSLATIONS)
    localize.load(QLocale::system().name(), "sipwitchqt-desktop", "_",
        DEBUG_TRANSLATIONS);
#elif defined(Q_OS_MAC)
    localize.load(QLocale::system().name(), "sipwitchqt-desktop", "_",
        Args::exePath("../Translations"));
#elif defined(Q_OS_WIN)
    localize.load(QLocale::system().name(), "sipwitchqt-desktop", "_",
        Args::exePath("./Translations"));
#else
    localize.load(QLocale::system().name(), "sipwitchqt-desktop", "_",
        Args::exePath("../share/" TRANSLATIONS_OUTPUT_DIRECTORY));
#endif
    if(!localize.isEmpty())
        QApplication::installTranslator(&localize);

#ifdef Q_OS_MAC
    QFile style(":/styles/macos.css");
#else
    QFile style(":/styles/desktop.css");
#endif
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
#ifndef STARTUP_MINIMIZED
        {{"minimize"}, "Minimize if logged in"},
#endif
        {{"notray"}, "Disable tray icon"},
        {{"reset"}, "Reset client config"},
    });

#ifdef SIGHUP
    ::signal(SIGHUP, signal_handler);
#endif
#ifdef SIGKILL
    ::signal(SIGKILL, signal_handler);
#endif
    ::signal(SIGINT, signal_handler);
    ::signal(SIGTERM, signal_handler);

#ifdef Q_OS_MAC
    disable_nap();
#endif

    args.process(app);

// This is really for autostart at login, as we need a delay for desktop
// to finish coming up before we try to add the dbus menu or tray...
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
#ifdef STARTUP_MINIMIZED
    ::sleep(3)
#else
    if(args.isSet("minimize"))
        ::sleep(3);
#endif
#endif

    Desktop w(args);
    int status = QApplication::exec();
    if(!result)
        result = status;
    return result;
}
