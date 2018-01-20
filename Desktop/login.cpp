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

#include <QHostInfo>

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

#ifndef NDEBUG
    ui.server->setText("sip:127.0.0.1:4060");
#endif

    UString hostname = QHostInfo::localHostName();
    int pos = hostname.indexOf('.');
    if(pos)
        hostname = hostname.left(pos);
    if(hostname.isLabel())
        ui.labels->addItem(hostname);
}

void Login::enter()
{
    ui.secret->setText("");
    ui.identity->setText("");
    ui.identity->setFocus();
}

void Login::badIdentity()
{
    ui.identity->setFocus();
}

void Login::badPassword()
{
    ui.secret->setFocus();
}

QVariantHash Login::credentials()
{
    QVariantHash cred;
    Contact uri(ui.identity->text(), ui.server->text());
    UString ext, label;

    if(!ui.secret->text().isEmpty())
        cred["secret"] = ui.secret->text();

    if(uri && uri.host().toLower() != "none") {
        cred["server"] = uri.host();
        cred["port"] = uri.port();
        if(!uri.user().isEmpty())
            ext = uri.user();
    }

    cred["realm"] = cred["type"] = "unknown";
    cred["label"] = label = ui.labels->currentText().toLower();
    cred["user"] = ext;
    cred["display"] = "";
    cred["extension"] = ext.toInt();

    qDebug() << "LOGIN CREDENTIALS" << cred;

    if(ext.isEmpty() || cred["server"].isNull()) {
        ui.identity->setFocus();
        desktop->error(tr("No identity specified"));
        return QVariantHash();
    }

    if(!ext.isNumber()) {
        ui.identity->setFocus();
        desktop->error(tr("Invalid extension #"));
        return QVariantHash();
    }

    if(ui.secret->text().isEmpty()) {
        ui.secret->setFocus();
        desktop->error(tr("No password entered"));
        return QVariantHash();
    }

    if(!label.isLabel()) {
        ui.labels->setFocus();
        desktop->error(tr("Invalid label used"));
        return QVariantHash();
    }

    return cred;
}

