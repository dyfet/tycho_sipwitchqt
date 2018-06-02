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

#include "newgroup.hpp"
#include "ui_newgroup.h"

#include <Desktop/desktop.hpp>
#include <QCryptographicHash>
#include <QHash>

static Ui::NewGroup ui;

NewGroup::NewGroup(Desktop *parent, Connector *connection) :
QDialog(parent, Qt::Popup|Qt::WindowTitleHint|Qt::WindowCloseButtonHint)
{
    ui.setupUi(static_cast<QDialog*>(this));
    connector = connection;
    auto creds = parent->credentials();

    connect(ui.cancelButton, &QPushButton::clicked, this, [=] {
        parent->closeDialog();
    });

    connect(ui.addButton, &QPushButton::clicked, this, &NewGroup::add);
    
    connect(ui.identity, &QLineEdit::returnPressed, this, [] {
        if(!ui.identity->text().isEmpty()) {
            ui.extension->setFocus();
        }
    });

    connect(ui.extension, &QLineEdit::returnPressed, this, [] {
        if(!ui.extension->text().isEmpty())
            ui.name->setFocus();
    });

    connect(ui.name, &QLineEdit::returnPressed, this, [] {
        if(ui.name->text().isEmpty())
            ui.name->setText(ui.identity->text());
        ui.type->setFocus();
    });

    connect(parent, &Desktop::changeConnector, [=](Connector *connection) {
        if(!connection)
            parent->closeDialog();
        else
            connector = connection;
    });

    ui.identity->setFocus();
    show();
}

void NewGroup::closeEvent(QCloseEvent *event)
{
    event->ignore();
//    Desktop::instance()->closeDialog();
}

void NewGroup::add()
{
    auto user = ui.identity->text();
    auto number = ui.extension->text().toInt();
    auto name = ui.name->text();
    auto type = ui.type->currentText().toUpper();

    if(name.isEmpty())
        name = user;

    if(user.isEmpty()) {
        emit error(tr("Identity missing for new group"));
        ui.identity->setFocus();
        return;
    }

    if(ContactItem::allUsers().contains(user)) {
        emit error(tr("Identity \"") + user + tr("\" already in use"));
        ui.identity->setFocus();
        ui.identity->clear();
        return;
    }

    if(number < 100 || number > 699) {
        emit error(tr("Invalid or missing extension"));
        ui.extension->setFocus();
        return;
    }

    if(Phonebook::find(number) != nullptr) {
        emit error(tr("Extension \"") + QString::number(number) + tr("\" already in use"));
        ui.extension->setFocus();
        ui.extension->clear();
        return;
    }

    QJsonObject json;

    json = {
        {"f", name},
        {"a", user},
        {"t", type},
        {"n", number},
    };

    qDebug() << "JSON" << json;
    QJsonDocument jdoc(json);
    connector->createAuthorize(UString::number(number), jdoc.toJson(QJsonDocument::Compact));
    Desktop::instance()->closeDialog();
}
