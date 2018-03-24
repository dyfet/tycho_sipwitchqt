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
#define CONST_CELLLIFT  3
#define CONST_CELLHIGHT 18

class Desktop final : public QMainWindow
{
	Q_OBJECT
    Q_DISABLE_COPY(Desktop)

public:
    typedef enum {
        INITIAL,
        OFFLINE,
        AUTHORIZING,
        CALLING,
        DISCONNECTING,
        ONLINE,
    } state_t;

    Desktop(const QCommandLineParser& args);

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

    inline QFont font() const {
        return baseFont;
    }

    QVariantHash storageCredentials() const {
        if(storage)
            return storage->credentials();
        else
            return QVariantHash();
    }

    // status bar functions...
    void warningMessage(const QString& msg, int timeout = 10000);
    void errorMessage(const QString& msg, int timeout = 30000);
    void statusMessage(const QString& msg, int timeout = 5000);
    void clearMessage();

    bool isCurrent(const QWidget *widget) const;
    bool notify(const QString& title, const QString& body, QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int timeout = 10000);

    inline QString appearance() const {
        return currentAppearance;
    }

    inline int expiration() const {
        return currentExpiration;
    }

    inline QDateTime expiresBefore() const {
        return QDateTime::fromMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch() - (currentExpiration * 1000l));
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
    QMenu *trayMenu, *dockMenu, *appMenu, *popup;
    bool restart_flag, front;
    QVariantHash currentCredentials;
    QString currentAppearance;
    int currentExpiration;
    QDialog *dialog;
    QFont baseFont;
    QString dbName;

    void closeEvent(QCloseEvent *event) final;
    QMenu *createPopupMenu() final;

    void listen(const QVariantHash &cred);
    void setState(state_t state);

    static QVariantHash Credentials;            // current credentials...
    static Desktop *Instance;
    static state_t State;

signals:
    void changeConnector(Connector *connector);
    void changeStorage(Storage *state);

public slots:
    void closeDeviceList(void);
    void openDeviceList(void);


    void initial(void);
    void dockClicked();
    void menuClicked();

    void closeDialog(void);
    void eraseLogout(void);
    void closeLogout(void);
    void openAbout(void);
    void openLogout(void);

    void showOptions(void);
    void showSessions(void);
    void showPhonebook(void);
    void exportDb(void);
    void importDb(void);

    void changeAppearance(const QString& appearance);
    void changeExpiration(const QString& expiration);

private slots:
    void authorized(const QVariantHash& creds); // server authorized
    void offline(void);                         // lost server connection
    void authorizing(void);                     // registering with server...
    void failed(int error_code);                // sip session fatal error

    void appState(Qt::ApplicationState state);
    void trayAction(QSystemTrayIcon::ActivationReason reason);
    void trayAway();

    void setBanner(const QString& banner);
    void shutdown();                            // application shutdown
};

#endif
