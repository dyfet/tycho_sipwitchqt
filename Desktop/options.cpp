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
#include "options.hpp"
#include "../Dialogs/devicelist.hpp"
#include "messages.hpp"
#include "sessions.hpp"
#include <QFontDialog>


#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif

static Ui::OptionsWindow ui;

Options::Options(Desktop *control) :
QWidget(), desktop(control)
{
    ui.setupUi(static_cast<QWidget *>(this));

//    connect(ui.resetButton, &QPushButton::pressed, control, &Desktop::doTheLogout);

//    connect(ui.fontSize, &QComboBox::currentTextChanged, control, &Desktop::changeFontValue);
    connect(ui.listDevices, &QPushButton::clicked, control, &Desktop::openDeviceList);
    connect(ui.ExportDb,&QPushButton::clicked,control,&Desktop::exportDb);
    connect(ui.ImportDb,&QPushButton::clicked,control,&Desktop::importDb);
    connect(ui.pickfontbutton, &QToolButton::clicked, this, &Options::fontDialog);
    connect(ui.resetFont, &QPushButton::clicked , control , &Desktop::resetFont);

    connect(ui.fontSize, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged) , [=](const int ind){

        int change = 0;
        if (ind == 0){ change = 8 ; }
        else if (ind == 1) { change = 10; }
        else if (ind == 2) { change = 13; }
        else if (ind == 3) { change = 15; }
        else if (ind == 4) { change = 18; }
        else if (ind == 5) { change = 21; }

        auto stupidfont = control->getCurrentFont();
        stupidfont.setPointSize(change);
        control->setTheFont(stupidfont);

    } );
}



void Options::enter()
{
    Toolbar::setTitle("");
    Toolbar::search()->setText("");
    Toolbar::search()->setPlaceholderText(tr("Disabled"));
    Toolbar::search()->setEnabled(false);
    ui.listDevices->setFocus();

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

void Options::fontDialog()
    {
    bool ok;
    QFont font = QFontDialog::getFont(
                &ok, QFont("Helvetica [Cronyx]", 10), this);
    if (ok) {
        desktop->setTheFont(font);
    } else {
       desktop->getCurrentFont() ;
    }
}
