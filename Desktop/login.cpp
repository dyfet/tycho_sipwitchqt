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

#ifdef QT_NO_DEBUG_OUTPUT
    ui.server->setText(desktop->config("login/server", QString("sip:_sipwitchqt.local")).toString());
#else
    ui.server->setText(desktop->config("login/server", QString("sip:127.0.0.1:4060")).toString());
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
    Toolbar::setTitle("");
    Toolbar::search()->setText("");
    Toolbar::search()->setPlaceholderText(tr("Disabled"));
    Toolbar::search()->setEnabled(false);

    // can pre-stuff default testing extension in testdata/settings.cfg this way...

    setEnabled(true);
    ui.secret->setText("");
    ui.identity->setText(desktop->config("login/extension", QString("")).toString());

    auto label = desktop->config("login/label").toString();
    if(label.length() > 0)
        ui.labels->setCurrentText(label);

    if(ui.identity->text().length() > 0)
        ui.secret->setFocus();
    else
        ui.identity->setFocus();
}

void Login::badIdentity()
{
    setEnabled(true);
    ui.identity->setFocus();
}

void Login::badPassword()
{
    setEnabled(true);
    ui.secret->setFocus();
}

void Login::badLabel()
{
    setEnabled(true);
    ui.label->setFocus();
}

QVariantHash Login::credentials()
{
    QVariantHash cred;
    Contact uri(ui.identity->text(), ui.server->text());
    UString ext, label;

    if(!ui.secret->text().isEmpty())
        cred["secret"] = ui.secret->text().toUtf8();

    if(uri && uri.host().toLower() != "none") {
        cred["host"] = QString(uri.host());
        cred["port"] = uri.port();
        if(!uri.user().isEmpty())
            ext = uri.user();
    }

    cred["realm"] = cred["type"] = "unknown";
    cred["label"] = ui.labels->currentText().toLower();
    cred["user"] = ext;
    cred["display"] = "";
    cred["extension"] = ext.toInt();
    cred["initialize"] = "label";

    label = (ui.labels->currentText().toLower()).toUtf8();

    qDebug() << "LOGIN CREDENTIALS" << cred;

    if(ext.isEmpty() || cred["host"].isNull()) {
        ui.identity->setFocus();
        desktop->errorMessage(tr("No ext or sip uri entered"));
        return QVariantHash();
    }

    if(!ext.isNumber()) {
        ui.identity->setFocus();
        desktop->errorMessage(tr("extension need to be 3 digit number"));
        return QVariantHash();
    }

    if(ext.toInt() < 100 || ext.toInt() > 699){
        ui.identity->setFocus();
        desktop->errorMessage(tr("Extension out of range choose in between 100-699"));
        return QVariantHash();
    }

    if(ui.secret->text().isEmpty()) {
        ui.secret->setFocus();
        desktop->errorMessage(tr("No password entered"));
        return QVariantHash();
    }

    if(!label.isLabel()) {
        ui.labels->setFocus();
        desktop->errorMessage(tr("Invalid label used"));
        return QVariantHash();
    }

    return cred;
}

