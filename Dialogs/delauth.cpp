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

#include "delauth.hpp"
#include "ui_delauth.h"

#include <Desktop/desktop.hpp>

static Ui::DelAuth ui;

DelAuth::DelAuth(Desktop *desktop, Connector *connection, const QString& id) :
QDialog(desktop, Qt::Popup|Qt::WindowTitleHint|Qt::WindowCloseButtonHint)
{
    ui.setupUi(static_cast<QDialog*>(this));
    connector = connection;

    auto items = ContactItem::findAuth(id);
    Q_ASSERT(items.count() > 0);

    connect(ui.cancelButton, &QPushButton::clicked, this, &DelAuth::reject);

    connect(ui.acceptButton, &QPushButton::clicked, this, [=] {
        connector->requestDeauthorize(id.toUtf8());
        desktop->closeDialog();
    });

    connect(desktop, &Desktop::changeConnector, [=](Connector *connection) {
        if(!connection)
            desktop->closeDialog();
        else
            connector = connection;
    });

    ui.about->setText(
        tr("Remove authorization for <font color=red>") + id + "</font>?<br><br>" +
        tr("This will permenantly remove <font color=red>") + QString::number(items.count()) + "</font> Extension(s).<br>"
    );

    show();
}

void DelAuth::closeEvent(QCloseEvent *event)
{
    event->ignore();
//    Desktop::instance()->closeDialog();
}

void DelAuth::reject()
{
    QDialog::reject();
    Desktop::instance()->closeDialog();
}
