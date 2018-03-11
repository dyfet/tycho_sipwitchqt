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

    connect(ui.closeButton, &QPushButton::clicked, parent, &Desktop::closeDeviceList);
//    connect(ui.cancelButton, &QPushButton::clicked, parent, &Desktop::closeDialog);
    show();
}

void DeviceList::closeEvent(QCloseEvent *event)
{
    Desktop::instance()->closeDialog();
}
