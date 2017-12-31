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
#include "toolbar.hpp"
#include "statusbar.hpp"

#include <QMainWindow>
#include <QString>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QSystemTrayIcon>
#include <QSettings>

#if defined(Q_OS_MAC)
#define CONFIG_FROM "tychosoft.com", "Antisipate"
#elif defined(Q_OS_WIN)
#define CONFIG_FROM "Tycho Softworks", "Antisipate"
#else
#define CONFIG_FROM "tychosoft.com", "antisipate"
#endif

class Desktop final : public QMainWindow
{
	Q_OBJECT
public:
    Desktop(bool tray, bool reset);
    virtual ~Desktop();

    inline static Desktop *instance() {
	    return Instance;
    }

    bool notify(const QString& title, const QString& body, QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int timeout = 10000);

    QWidget *extendToolbar(QToolBar *bar, QMenuBar *menu = nullptr);

private:
    Control *control;
    Toolbar *toolbar;
    Statusbar *statusbar;
    QSettings settings;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu, *dockMenu;
    bool restart_flag;

    QMenu *appMenu(const QString& id);

    static Desktop *Instance;
};

#endif
