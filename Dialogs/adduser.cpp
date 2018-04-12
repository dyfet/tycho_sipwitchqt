/*
 * Copyright (C) 2017 Tycho Softworks.
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

#include "adduser.hpp"
#include "ui_adduser.h"

#include <Desktop/desktop.hpp>
#include <QCryptographicHash>
#include <QHash>

static Ui::AddUser ui;

static QHash<UString, QCryptographicHash::Algorithm> digests = {
    {"MD5",     QCryptographicHash::Md5},
    {"SHA",     QCryptographicHash::Sha1},
    {"SHA1",    QCryptographicHash::Sha1},
    {"SHA2",    QCryptographicHash::Sha256},
    {"SHA256",  QCryptographicHash::Sha256},
    {"SHA512",  QCryptographicHash::Sha512},
    {"SHA-1",   QCryptographicHash::Sha1},
    {"SHA-256", QCryptographicHash::Sha256},
    {"SHA-512", QCryptographicHash::Sha512},
};

AddUser::AddUser(Desktop *parent, Connector *connection) :
QDialog(parent, Qt::Popup|Qt::WindowTitleHint|Qt::WindowCloseButtonHint)
{
    ui.setupUi(static_cast<QDialog*>(this));

    ui.authorization->clear();
    ui.authorization->addItems(ContactItem::allUsers());
    ui.secret->setEnabled(false);
    ui.fullName->setEnabled(false);
    ui.authorization->setFocus();

    connector = connection;
    auto creds = parent->credentials();
    realm = creds["realm"].toString();
    digest = creds["algorithm"].toString().toUpper();

    connect(ui.cancelButton, &QPushButton::clicked, parent, &Desktop::closeDialog);
    connect(ui.authorization, &QComboBox::editTextChanged, this, &AddUser::changedAuth);
    connect(ui.addButton, &QPushButton::pressed, this, &AddUser::add);
    show();

    qDebug() << "Add for realm" << realm << "using" << digest;
}

void AddUser::closeEvent(QCloseEvent *event)
{
    event->ignore();
//    Desktop::instance()->closeDialog();
}

void AddUser::changedAuth(const QString& text)
{
    if(ContactItem::allUsers().contains(text)) {
        ui.secret->setEnabled(false);
        ui.fullName->setEnabled(false);
    }
    else {
        ui.secret->setEnabled(true);
        ui.fullName->setEnabled(true);
    }
}

void AddUser::changeConnector(Connector *connection)
{
    if(!connection)
        Desktop::instance()->closeDialog();
    else
        connector = connection;
}

void AddUser::add()
{
    auto user = ui.authorization->currentText();
    auto number = ui.extension->text().toInt();
    auto secret = ui.secret->text();
    auto name = ui.fullName->text();
    auto type = ui.type->currentText().toUpper();
    auto createAuth = !ContactItem::allUsers().contains(user);

    if(createAuth) {
        if(secret.isEmpty()) {
            emit error(tr("Password missing for new user"));
            return;
        }
        if(name.isEmpty()) {
            emit error(tr("Name missing for new user"));
            return;
        }
    }

    if(ContactItem::allGroups().contains(user)) {
        emit error("\"" + user +"\" " + tr("is defined in a group"));
        return;
    }

    if(number < 100 || number > 699) {
        emit error("Invalid or missing extension");
        return;
    }
    QJsonObject json;

    if(createAuth) {
        UString ha1 = user + ":" + realm + ":" + secret;
        secret = QCryptographicHash::hash(ha1, digests[digest]).toHex();
        json = {
            {"s", secret},
            {"r", realm},
            {"d", digest},
            {"f", name},
            {"a", user},
            {"t", type},
            {"n", number},
        };
    }
    else {
        json = {
            {"f", name},
            {"e", user},
            {"t", type},
            {"n", number},
        };
    }
    qDebug() << "JSON" << json;
    QJsonDocument jdoc(json);
    connector->createAuthorize(UString::number(number), jdoc.toJson(QJsonDocument::Compact));
    Desktop::instance()->closeDialog();
}
