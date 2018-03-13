#include "devicelist.hpp"
#include "ui_devicelist.h"
#include <Desktop/desktop.hpp>
#include <Desktop/options.hpp>

static Ui::DeviceList ui;

DeviceList::DeviceList(Desktop *parent, Connector *connector) :
QDialog(parent, Qt::Popup|Qt::WindowTitleHint|Qt::WindowCloseButtonHint)
{
    ui.setupUi(static_cast<QDialog*>(this));

    if(connector != nullptr) {
        //TODO: connect to receive server reply...
        connector->requestDeviceList();
    }

    tableWidget.setColumnCount(6);
    tableWidget.setRowCount(1);

            connect(ui.closeButton, &QPushButton::clicked, parent, &Desktop::closeDeviceList);
    connect(connector, &Connector::changeDeviceList, this, [=](const QByteArray& json){
            if(!connector)
                return;
            auto jdoc = QJsonDocument::fromJson(json);
            auto list = jdoc.array();
        });
    show();
}

void DeviceList::closeEvent(QCloseEvent *event)
{
    Desktop::instance()->closeDialog();
}
