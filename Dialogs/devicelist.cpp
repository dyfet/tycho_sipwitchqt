#include "devicelist.hpp"
#include "ui_devicelist.h"
#include <Desktop/desktop.hpp>
#include <Desktop/options.hpp>

#include <QWidget>

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
        QTableWidget table(this);
        table.setColumnCount(6);
        table.setRowCount(1);

                    foreach(auto profile, list) {
                        auto json = profile.toObject();
                        foreach (value, json) {
                             table;
                        }

                    }

                });
    }

//    tableWidget.setColumnCount(6);
//    tableWidget.setRowCount(1);

    connect(ui.closeButton, &QPushButton::clicked, parent, &Desktop::closeDeviceList);
    show();
}



//void DeviceList::updateTable(const QJsonObject& json)
//{
//            auto jdoc = QJsonDocument::fromJson(json);
//            auto list = jdoc.array();

//            foreach(auto profile, list) {
//                localModel->updateContact(profile.toObject());
//            }

//}


void DeviceList::closeEvent(QCloseEvent *event)
{
    Desktop::instance()->closeDialog();
}
