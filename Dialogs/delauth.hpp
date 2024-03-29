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

#ifndef DELAUTH_HPP_
#define DELAUTH_HPP_

#include <QDialog>

#include "../Desktop/desktop.hpp"

class DelAuth final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(DelAuth)

public:
    DelAuth(Desktop *desktop, Connector *connection, const QString &id);

private:
    Connector *connector;

    void closeEvent(QCloseEvent *event) final;
    void reject() final;

signals:
    void error(const QString& text);
};

/*!
 * Ui Delete authorization dialog.
 * \file delauth.hpp
 */

/*!
 * \class DelAuth
 * \brief implements delete authorization dialog
 * This is used to delete an authorizing id on the client and server.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
