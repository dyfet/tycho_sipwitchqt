/*
 * Copyright (C) 2017 Tycho Softworks.
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

#ifndef NEWGROUP_HPP_
#define NEWGROUP_HPP_

#include <QDialog>

#include "../Desktop/desktop.hpp"

class NewGroup final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(NewGroup)

public:
    NewGroup(Desktop *parent, Connector *connection);

private:
    QString realm, digest;
    Connector *connector;

    void closeEvent(QCloseEvent *event) final;

signals:
    void error(const QString& text);

private slots:
    void add();
};

/*!
 * Ui NewGroup dialog.
 * \file NewGroup.hpp
 */

/*!
 * \class NewGroup
 * \brief implements creating a new group.
 * This is used to create a new group on the server.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
