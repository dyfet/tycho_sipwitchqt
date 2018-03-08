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
#include "ui_options.h"

#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif

static Ui::OptionsWindow ui;

Options::Options(Desktop *control) :
QWidget(), desktop(control)
{
    ui.setupUi(static_cast<QWidget *>(this));

//    connect(ui.resetButton, &QPushButton::pressed, control, &Desktop::doTheLogout);
}

void Options::enter()
{
    Toolbar::setTitle("");
    Toolbar::search()->setText("");
    Toolbar::search()->setPlaceholderText(tr("Disabled"));
    Toolbar::search()->setEnabled(false);
    ui.listButton->setFocus();

    auto storage = Storage::instance();
    Q_ASSERT(storage != nullptr);

    ui.appearance->setCurrentText(desktop->appearance());

    auto creds = storage->credentials();
    ui.displayName->setText(creds["display"].toString());
    ui.server->setText(creds["host"].toString());
    ui.secret->setText("");

    /* Need explanation I think we are removing this not functional
    // part of code later as we will have different expiration options
    // that will work*/
    int expiresIndex = 0;
    auto expires = desktop->expiration();
    switch(expires /= 86400) {
    case 30:
        expiresIndex = 2;
        break;
    case 14:
        expiresIndex = 1;
        break;
    default:
        break;
    }

    ui.expires->setCurrentIndex(expiresIndex);

    connect(ui.appearance, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), desktop, &Desktop::changeAppearance);

    connect(ui.expires, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), desktop, &Desktop::changeExpiration);
}
