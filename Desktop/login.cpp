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

#include "desktop.hpp"
#include "ui_login.h"

#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif

static Ui::LoginWindow ui;

Login::Login(Desktop *control) :
QWidget(), desktop(control)
{
    ui.setupUi(static_cast<QWidget *>(this));
    connect(ui.loginButton, &QPushButton::pressed, desktop, &Desktop::initial);
    connect(ui.secret, &QLineEdit::returnPressed, desktop, &Desktop::initial);
//    connect(ui.identity, &QLineEdit::returnPressed, ui.secret, &QLineEdit::setFocus);

#ifndef NDEBUG
    ui.server->setText("sip:127.0.0.1:4060");
#endif

    enter();
}

void Login::enter()
{
    ui.secret->setText("");
    ui.identity->setText("");
    ui.identity->setFocus();
}

QVariantHash Login::credentials()
{
    QVariantHash cred;
    Contact uri(ui.identity->text(), ui.server->text());

    if(!ui.secret->text().isEmpty())
        cred["secret"] = ui.secret->text();

    if(uri && uri.host().toLower() != "none") {
        cred["server"] = uri.host();
        cred["port"] = uri.port();
        if(!uri.user().isEmpty())
            cred["userid"] = uri.user();
    }

    cred["realm"] = cred["type"] = "unknown";
    qDebug() << "LOGIN CREDENTIALS" << cred;

    if(cred["secret"].isNull()) {
        ui.secret->setFocus();
        desktop->error(tr("No password entered"));
        return QVariantHash();
    }

    if(cred["userid"].isNull() || cred["server"].isNull()) {
        ui.identity->setFocus();
        desktop->error(tr("No identity specified"));
        return QVariantHash();
    }

    return cred;
}

