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

#ifndef ADDUSER_HPP_
#define ADDUSER_HPP_

#include <QDialog>

#include "../Desktop/desktop.hpp"

class AddUser final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(AddUser)

public:
    AddUser(Desktop *parent, Connector *connection);

private:
    QString realm, digest;
    Connector *connector;

    void closeEvent(QCloseEvent *event) final;

signals:
    void error(const QString& text);

private slots:
    void changedAuth(const QString& text);
    void secretChanged(const QString& text);
    void add();
};

/*!
 * Ui AddUser dialog.
 * \file AddUser.hpp
 */

/*!
 * \class AddUser
 * \brief implements creating user and extension.
 * This is used to create a new user on the server.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
