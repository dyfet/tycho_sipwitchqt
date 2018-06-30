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
    QTableWidget tableWidget (int rows, int cols, DeviceList *parent = nullptr);

//    QTableWidget table(this);


//    void DeviceList::updateTable(const QJsonObject& json);
private:
//    int rows = 1;
//    int column = 6;
    void closeEvent(QCloseEvent *event) final;
    void reject() final;
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
