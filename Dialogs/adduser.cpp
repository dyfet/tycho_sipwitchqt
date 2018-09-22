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

#include "../Common/crypto.hpp"
#include "adduser.hpp"
#include "ui_adduser.h"

static Ui::AddUser ui;

AddUser::AddUser(Desktop *parent, Connector *connection) :
QDialog(parent, Qt::Popup|Qt::WindowTitleHint|Qt::WindowCloseButtonHint)
{
    QStringList users;
    users << "";
    users << ContactItem::allUsers();

    ui.setupUi(static_cast<QDialog*>(this));
    ui.authorization->clear();
    ui.authorization->addItems(users);
    ui.secret->setEnabled(false);
    ui.fullName->setEnabled(false);
    ui.authorization->setFocus();

    connector = connection;
    auto creds = Desktop::credentials();
    realm = creds["realm"].toString();
    digest = creds["algorithm"].toString().toUpper();

    connect(ui.cancelButton, &QPushButton::clicked, this, &AddUser::reject);
    connect(ui.authorization, &QComboBox::editTextChanged, this, &AddUser::changedAuth);
    connect(ui.addButton, &QPushButton::pressed, this, &AddUser::add);
    connect(ui.confirm, &QLineEdit::textChanged, this, &AddUser::secretChanged);
    connect(ui.secret, &QLineEdit::textChanged, this, &AddUser::secretChanged);

    connect(ui.secret, &QLineEdit::returnPressed, this, []{
        if(!ui.secret->text().isEmpty())
            ui.confirm->setFocus();
    });

    connect(ui.confirm, &QLineEdit::returnPressed, this, []{
        if(!ui.confirm->text().isEmpty())
            ui.extension->setFocus();
    });

    connect(ui.extension, &QLineEdit::returnPressed, this, []{
        if(!ui.extension->text().isEmpty())
            ui.type->setFocus();
    });

    connect(parent, &Desktop::changeConnector, [=](Connector *connection) {
        if(!connection)
            parent->closeDialog();
        else
            connector = connection;
    });

    show();

    qDebug() << "Add for realm" << realm << "using" << digest;
}

void AddUser::closeEvent(QCloseEvent *event)
{
    event->ignore();
}

void AddUser::reject()
{
    QDialog::reject();
    Desktop::instance()->closeDialog();
}

void AddUser::changedAuth(const QString& text)
{
    if(ContactItem::allUsers().contains(text)) {
        ui.secret->setEnabled(false);
        ui.confirm->setEnabled(false);
        ui.fullName->setEnabled(false);
    }
    else {
        ui.secret->setEnabled(true);
        ui.confirm->setEnabled(true);
        ui.fullName->setEnabled(true);
    }
}

void AddUser::secretChanged(const QString& text)
{
    Q_UNUSED(text);
    if(ui.secret->text() == ui.confirm->text())
        ui.confirm->setStyleSheet("background: white; padding: 1px;");
    else
        ui.confirm->setStyleSheet("color: red; background: white; padding: 1px");
}

void AddUser::add()
{
    auto user = ui.authorization->currentText();
    auto number = ui.extension->text().toInt();
    auto secret = ui.secret->text();
    auto confirm = ui.confirm->text();
    auto name = ui.fullName->text();
    auto type = ui.type->currentText().toUpper();
    auto createAuth = !ContactItem::allUsers().contains(user);

    if(createAuth) {
        if(name.isEmpty()) {
            emit error(tr("Name missing for new user"));
            ui.authorization->setFocus();
            return;
        }

        if(secret.isEmpty()) {
            emit error(tr("Password missing for new user"));
            ui.secret->setFocus();
            return;
        }

        if(secret != confirm) {
            emit error(tr("Password does not match confirmation"));
            ui.confirm->setFocus();
            return;
        }
    }

    if(number < 100 || number > 699) {
        emit error("Invalid or missing extension");
        ui.extension->setFocus();
        return;
    }
    QJsonObject json;

    if(createAuth) {
        UString ha1 = user + ":" + realm + ":" + secret;
        secret = QCryptographicHash::hash(ha1, Crypto::digests[digest]).toHex();
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
