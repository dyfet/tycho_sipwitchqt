#ifndef DEVICELIST_HPP
#define DEVICELIST_HPP
#include <QTableWidget>
#include <QDialog>

#include "../Desktop/desktop.hpp"

class DeviceList final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(DeviceList)
public:
    DeviceList(Desktop *parent, Connector *connector);
    QTableWidget tableWidget (int rows, int cols, DeviceList *parent = 0);

//    QTableWidget table(this);


//    void DeviceList::updateTable(const QJsonObject& json);
private:
//    int rows = 1;
//    int column = 6;
    void closeEvent(QCloseEvent *event) final;
//    QTableWidget tableWidget;
};

/*!
 * Ui list devices.
 * \file devicelist.hpp
 */

/*!
 * \class devicelist
 * \brief implements list devices that are connected.
 * This is used to list all devices that are connected on current extension
 * \author Marko Pavlovic <marko.shiva.pavlovic@gmail.com>
 * <in1t3r@protonmail.com>
 */

#endif // DEVICELIST_HPP
