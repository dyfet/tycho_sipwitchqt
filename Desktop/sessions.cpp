/*
 * Copyright 2017 Tycho Softworks.
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

#include "desktop.hpp"
#include "ui_sessions.h"

#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif

static Ui::SessionsWindow ui;

Sessions::Sessions(Desktop *control) :
QWidget(), desktop(control)
{
    ui.setupUi(static_cast<QWidget *>(this));
    ui.contacts->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui.messages->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(Toolbar::search(), &QLineEdit::returnPressed, this, &Sessions::search);
}

void Sessions::enter()
{
    Toolbar::search()->setEnabled(true);
    ui.input->setFocus();
}

void Sessions::search()
{
    if(!desktop->isCurrent(this))
        return;
}
