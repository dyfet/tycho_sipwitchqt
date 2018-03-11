#ifndef DEVICELIST_HPP
#define DEVICELIST_HPP

#include <QDialog>

#include "../Desktop/desktop.hpp"

class DeviceList final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(DeviceList)
public:
    DeviceList(Desktop *parent = nullptr);
private:
    void closeEvent(QCloseEvent *event) final;
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
