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

#ifndef DESKTOP_HPP_
#define	DESKTOP_HPP_

#include "../Common/compiler.hpp"
#include "../Connect/control.hpp"
#include "../Connect/listener.hpp"
#include "../Connect/connector.hpp"
#include "../Connect/storage.hpp"
#include "toolbar.hpp"
#include "statusbar.hpp"

#include "login.hpp"
#include "sessions.hpp"
#include "phonebook.hpp"
#include "options.hpp"

#include <QMainWindow>
#include <QString>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QSystemTrayIcon>
#include <QSettings>
#include <QGuiApplication>

// config paths and names

#if defined(DESKTOP_PREFIX)
#define CONFIG_FROM DESKTOP_PREFIX "/settings.cfg", QSettings::IniFormat
#elif defined(Q_OS_MAC)
#define CONFIG_FROM "tychosoft.com", "SipWitchQt"
#elif defined(Q_OS_WIN)
#define CONFIG_FROM "Tycho Softworks", "SipWitchQt"
#else
#define CONFIG_FROM "tychosoft.com", "sipwitchqt"
#endif

// magic ui related constants...

#define CONST_CLICKTIME 120
#define CONST_CLICKCOLOR "lightgray"

class Desktop final : public QMainWindow
{
//    friend class Options;
	Q_OBJECT
    Q_DISABLE_COPY(Desktop)

public:
    enum state_t {
        INITIAL,
        OFFLINE,
        AUTHORIZING,
        CALLING,
        DISCONNECTING,
        ONLINE,
    };

    explicit Desktop(const QCommandLineParser& args);

    bool isConnected() const {
        return connector != nullptr;
    }

    bool isActive() const {
        return listener != nullptr;
    }

    bool isOpened() const {
        return storage != nullptr;
    }

    bool isLogin() const {
        return isCurrent(login);
    }

    bool isSession() const {
        return isCurrent(sessions);
    }

    bool isProfile() const {
        return isCurrent(phonebook);
    }

    bool isOptions() const {
        return isCurrent(options);
    }

    QVariantHash storageCredentials() const {
        if(storage)
            return storage->credentials();
        else
            return QVariantHash();
    }

    inline bool checkRoster() const {
        return updateRoster;
    }

    inline void clearRoster() {
        updateRoster = false;
    }

    inline QFont getCurrentFont() {                                      // get current value from the settings.cfg
        auto font = settings.value("font").toString();
        if (font.isEmpty() || !(baseFont.fromString(font)))  // So font is now client related and persistent
            baseFont = QGuiApplication::font();
        return baseFont;
    }

    inline QFont getBasicFont() {                      // simple getter for cases that current font do not work.
        baseFont = QGuiApplication::font();
        return baseFont;
    }


    inline void setTheFont(const QFont& font) {         // set the global value of a basefont
        baseFont = font;
        settings.setValue("font", font.toString());
        emit changeFont();
    }

    // status bar functions...
    void warningMessage(const QString& msg, int timeout = 10000);
    void errorMessage(const QString& msg, int timeout = 30000);
    void statusMessage(const QString& msg, int timeout = 5000);
    void clearMessage();

    void notification(const QString& title, const QString& body);

    void setSelf(const QString& text);
    bool isCurrent(const QWidget *widget) const;
    bool notify(const QString& title, const QString& body, QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int timeout = 10000);

    bool isAutoAnswer() const {
        return autoAnswer;
    }

    void enableAutoAnswer(bool enable = true) {
        autoAnswer = enable;
        settings.setValue("autoanswer", enable);
    }

    void disableAutoAnswer(bool disable = true) {
        return enableAutoAnswer(!disable);
    }

    bool isDesktopNotify() const {
        return desktopNotify;
    }

    void enableDesktopNotify(bool enable = true) {
        desktopNotify = enable;
        settings.setValue("notify-desktop", enable);
    }

    void disableDesktopNotify(bool disable = true) {
        return enableDesktopNotify(!disable);
    }

    bool isAudioNotify() const {
        return audioNotify;
    }

    void enableAudioNotify(bool enable = true) {
        audioNotify = enable;
        settings.setValue("notify-audio", enable);
    }

    void disableAudioNotify(bool disable = true) {
        return enableAudioNotify(!disable);
    }

    inline QString peering() const {
        if(!listener)
            return "offline";
        return peeringAddress;
    }

    inline QString appearance() const {
        return currentAppearance;
    }

    inline int expiration() const {
        return currentExpiration;
    }

    inline QDateTime expiresBefore() const {
        return QDateTime::fromMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch() - (static_cast<qint64>(currentExpiration) * 1000l));
    }

    inline QVariant config(const QString& path, const QVariant& value = QVariant()) {
        return settings.value(path, value);
    }

    inline Listener *getListener() const {
        return listener;
    }

    inline static Desktop *instance() {
        return Instance;
    }

    inline static state_t state() {
        return State;
    }

    inline static const QVariantHash credentials() {
        return Credentials;
    }

    inline static bool isAdmin() {
        return (State == Desktop::ONLINE && Credentials["privs"].toString() == "sysadmin");
    }

    inline static bool isOperator() {
        return (State == Desktop::ONLINE && Credentials["privs"].toString() == "operator");
    }

    static void setUnread(unsigned unread);

private:
    Login *login;
    Sessions *sessions;
    Phonebook *phonebook;
    Options *options;
    Control *control;
    Listener *listener;
    Connector *connector;
    Storage *storage;
    Toolbar *toolbar;
    Statusbar *statusbar;
    QSettings settings;
    QSystemTrayIcon *trayIcon;
    QMenuBar *appBar;
    QTimer dailyTimer;
    QMenu *trayMenu, *dockMenu, *appMenu, *popup;
    bool front;
    QVariantHash currentCredentials;
    QString currentAppearance;
    QString peeringAddress;
    int currentExpiration;
    QDialog *dialog;
    QFont baseFont;
    int baseHeight;
    QString dbName;
    bool powerReconnect;
    bool updateRoster;
    bool offlineMode;
    bool autoAnswer;
    bool desktopNotify, audioNotify;

    void closeEvent(QCloseEvent *event) final;
    QMenu *createPopupMenu() final;

    void listen(const QVariantHash &cred);
    void setState(state_t state);

    static QVariantHash Credentials;            // current credentials...
    static Desktop *Instance;
    static state_t State;

signals:
    void changeConnector(Connector *connector);
    void changeListener(Listener *listener);
    void changeStorage(Storage *state);
    void changeSelf(const QString& text);
    void changeFont();
    void dailyEvent();
    void logout();

public slots:
    void openDeviceList();

    void initial();
    void dockClicked();
    void powerSuspend();
    void powerResume();
    void menuClicked();

    void closeDialog();
    void eraseLogout();
    void closeLogout();
    void openAbout();
    void openLogout();
    void openAddUser();
    void openNewGroup();
    void openDelAuth(const UString& id);

    void showOptions();
    void showSessions();
    void showPhonebook();
    void exportDb();
    void importDb();

    void changePassword(const QString& password);
    void changeAppearance(const QString& appearance);
    void changeExpiration(int expire);
    void removeUser(const QString& user);   // callback from listener notify

private slots:
    void authorized(const QVariantHash& creds); // server authorized
    void offline();                         // lost server connection
    void authorizing();                     // registering with server...
    void failed(int error_code);                // sip session fatal error
    void statusResult(int status, const QString& text);

    void appState(Qt::ApplicationState state);
    void trayAction(QSystemTrayIcon::ActivationReason reason);
    void trayAway();
    void shutdown(); // application shutdown
};

#endif
