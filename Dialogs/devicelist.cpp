#include "devicelist.hpp"
#include "ui_devicelist.h"
#include <Desktop/desktop.hpp>
#include <Desktop/options.hpp>

#include <QWidget>

#include <iostream>
static Ui::DeviceList ui;

DeviceList::DeviceList(Desktop *parent, Connector *connector) :
QDialog(parent, Qt::Popup|Qt::WindowTitleHint|Qt::WindowCloseButtonHint)
{
    ui.setupUi(static_cast<QDialog*>(this));
    if(connector != nullptr) {
        connector->requestDeviceList();

        connect(connector, &Connector::changeDeviceList, this, [=](const QByteArray& json){
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

                        auto json = profile.toObject();
                        auto endpoint = json.value("e").toString();
                        auto number = json.value("n").toString();
                        auto label = json.value("u").toString();
                        auto lastOnline = json.value("o").toString();
                        auto registered = json.value("r").toString();
                        auto agent = json.value("a").toString();
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

    connect(ui.closeButton, &QPushButton::clicked, parent, &Desktop::closeDeviceList);
    show();
}

void DeviceList::closeEvent(QCloseEvent *event)
{
    event->ignore();

//    Desktop::instance()->closeDialog();
}
