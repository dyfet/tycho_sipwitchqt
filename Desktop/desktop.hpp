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

#if defined(DESKTOP_PREFIX)
#define CONFIG_FROM DESKTOP_PREFIX "/settings.cfg", QSettings::IniFormat
#elif defined(Q_OS_MAC)
#define CONFIG_FROM "tychosoft.com", "Antisipate"
#elif defined(Q_OS_WIN)
#define CONFIG_FROM "Tycho Softworks", "Antisipate"
#else
#define CONFIG_FROM "tychosoft.com", "antisipate"
#endif

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

    Desktop(bool tray, bool reset);
    virtual ~Desktop();

    bool isConnected() {
        return connected;
    }

    bool isActive() {
        return listener != nullptr;
    }

    bool isOpened() {
        return storage != nullptr;
    }

    bool notify(const QString& title, const QString& body, QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int timeout = 10000);

    QWidget *extendToolbar(QToolBar *bar, QMenuBar *menu = nullptr);

    inline static Desktop *instance() {
        return Instance;
    }

    inline static state_t state() {
        return State;
    }

private:
    Login *login;
    Sessions *sessions;
    Phonebook *phonebook;
    Options *options;
    Control *control;
    Listener *listener;
    Storage *storage;
    Toolbar *toolbar;
    Statusbar *statusbar;
    QSettings settings;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu, *dockMenu, *appMenu;
    bool restart_flag, connected;

    void listen();

    // status bar functions...
    void warning(const QString& msg);
    void error(const QString& msg);
    void status(const QString& msg);
    void clear();

    static Desktop *Instance;
    static state_t State;

public slots:
    void online(void);              // server authorized
    void offline(void);             // lost server connection
    void authorizing(void);         // registering with sip server...
    void initial(void);             // initial connection

private slots:
    void gotoOptions(void);
    void gotoSessions(void);
    void gotoPhonebook(void);
};

#endif
