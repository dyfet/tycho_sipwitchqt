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

#ifndef RESETDB_HPP_
#define RESETDB_HPP_

#include <QDialog>

#include "../Desktop/desktop.hpp"

class ResetDb final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(ResetDb)

public:
    ResetDb(Desktop *parent = nullptr);

private:
    void closeEvent(QCloseEvent *event) final;
};

/*!
 * Ui resetdb dialog.
 * \file resetdb.hpp
 */

/*!
 * \class ResetDb
 * \brief implements reset database dialog.
 * This is used to reset the sipwitchqt desktop application back to it's
 * initial state and allow a new login to occur without past data.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
