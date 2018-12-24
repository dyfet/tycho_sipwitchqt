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

#include "../Desktop/options.hpp"
#include "devicelist.hpp"
#include "ui_devicelist.h"

#include <QWidget>
#include <iostream>

static Ui::DeviceList ui;

DeviceList::DeviceList(Desktop *parent, Connector *connector) :
QDialog(parent, Qt::Popup|Qt::WindowTitleHint|Qt::WindowCloseButtonHint)
{
    ui.setupUi(static_cast<QDialog*>(this));
    if(connector != nullptr) {
        connector->requestDeviceList();

        connect(connector, &Connector::changeDeviceList, this, [this](const QByteArray& json){
            auto jdoc = QJsonDocument::fromJson(json);
            auto list = jdoc.array();
            ui.table->setColumnCount(6);
            int totalCol = ui.table->columnCount();
            ui.table->setRowCount(1);
            int column = 0;
            int row = 0;
            ui.table->setEditTriggers(QAbstractItemView::NoEditTriggers);

            ui.table->setSizeAdjustPolicy(QTableWidget::AdjustToContents);
            foreach(auto profile, list) {
                auto obj = profile.toObject();
                auto endpoint = obj.value("e").toString();
                auto number = obj.value("n").toString();
                auto label = obj.value("u").toString();
                auto lastOnline = obj.value("o").toString();
                auto registered = obj.value("r").toString();
                auto agent = obj.value("a").toString();
                QStringList strArr = {endpoint, number,label,agent, registered, lastOnline};
                for (column = 0; column < totalCol; column++){
                    ui.table->setItem(row,column, new QTableWidgetItem(strArr[column]));
                }
                ui.table->setRowCount(ui.table->rowCount() + 1);
                row++;
            }
            ui.table->removeRow(ui.table->rowCount() - 1);
//            auto count = ui.table->rowCount();
            ui.connectedDevices->setText(QString::number(ui.table->rowCount()));
        });
    }
    connect(ui.closeButton, &QPushButton::clicked, parent, &Desktop::closeDialog);
    show();
}

void DeviceList::reject()
{
    QDialog::reject();
    Desktop::instance()->closeDialog();
}

void DeviceList::closeEvent(QCloseEvent *event)
{
    event->ignore();

//    Desktop::instance()->closeDialog();
}
