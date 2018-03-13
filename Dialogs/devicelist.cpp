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
        ui.table->setRowCount(1);

                    foreach(auto profile, list) {
                        auto json = profile.toObject();
                        auto endpoint = json.value("e").toString();
                        auto number = json.value("n").toString();
                        auto label = json.value("u").toString();
                        auto lastOnline = json.value("o").toString();
                        auto registered = json.value("r").toString();
                        auto agent = json.value("a").toString();


                        ui.table->setRowCount(ui.table->currentRow() + 1);
                        ui.table->setSizeAdjustPolicy(QTableWidget::AdjustToContents);
                        ui.table->setItem(1, 1, new QTableWidgetItem(endpoint));
                        ui.table->setItem(1, 2, new QTableWidgetItem(number));
                        ui.table->setItem(1, 3, new QTableWidgetItem(label));
                        ui.table->setItem(1, 4, new QTableWidgetItem(agent));
                        ui.table->setItem(1, 5, new QTableWidgetItem(registered));
                        ui.table->setItem(1, 6, new QTableWidgetItem(lastOnline));
                    }
                });
}
    connect(ui.closeButton, &QPushButton::clicked, parent, &Desktop::closeDeviceList);
    show();
}


void DeviceList::closeEvent(QCloseEvent *event)
{
    Desktop::instance()->closeDialog();
}
