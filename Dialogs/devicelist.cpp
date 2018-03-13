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
        //TODO: connect to receive server reply...
        connector->requestDeviceList();
        connect(connector, &Connector::changeDeviceList, this, [=](const QByteArray& json){
                    auto jdoc = QJsonDocument::fromJson(json);
                    auto list = jdoc.array();
            ui.table->setColumnCount(6);
            int totalCol = ui.table->columnCount();
            ui.table->setRowCount(1);
            int column = 0;
            int row = 0;


            foreach(auto profile, list) {

                        auto json = profile.toObject();
                        auto endpoint = json.value("e").toString();
                        auto number = json.value("n").toString();
                        auto label = json.value("u").toString();
                        auto lastOnline = json.value("o").toString();
                        auto registered = json.value("r").toString();
                        auto agent = json.value("a").toString();
                        QStringList strArr = {endpoint, number,label,agent, registered, lastOnline};
                        ui.table->setSizeAdjustPolicy(QTableWidget::AdjustToContents);
                        for (column = 0; column < totalCol; column++){
                            ui.table->setItem(row,column, new QTableWidgetItem(strArr[column]));
                        }
                        ui.table->setRowCount(ui.table->rowCount() + 1);
                        row++;
                    }
            ui.table->removeRow(ui.table->rowCount());
//            auto count = ui.table->rowCount();
            auto count = row;

            ui.connectedDevices->setText(QString(count));
                });
}

    connect(ui.closeButton, &QPushButton::clicked, parent, &Desktop::closeDeviceList);
    show();
}

//QTableWidget tableWidget (int rows, int column, DeviceList *parent = 0)
//{

//}


void DeviceList::closeEvent(QCloseEvent *event)
{
    Desktop::instance()->closeDialog();
}
