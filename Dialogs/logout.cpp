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

#include "logout.hpp"
#include "ui_logout.h"

static Ui::Logout ui;

Logout::Logout(Desktop *parent) :
QDialog(parent, Qt::Popup|Qt::WindowTitleHint|Qt::WindowCloseButtonHint)
{
    ui.setupUi(static_cast<QDialog*>(this));

    connect(ui.logoutButton, &QPushButton::clicked, parent, &Desktop::closeLogout);
    connect(ui.eraseButton, &QPushButton::clicked, parent, &Desktop::eraseLogout);
    connect(ui.cancelButton, &QPushButton::clicked, this, &Logout::reject);
    show();
}

void Logout::closeEvent(QCloseEvent *event)
{
    event->ignore();
//    Desktop::instance()->closeDialog();
}

void Logout::reject()
{
    QDialog::reject();
    Desktop::instance()->closeDialog();
}
