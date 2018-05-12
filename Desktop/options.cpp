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
    ui.appearance->setCurrentText(desktop->appearance());

    int expireValue = 60;
    int currentExpiration = desktop->expiration();
    switch (currentExpiration / expireValue) {
    case 3:
        ui.expires->setCurrentIndex(0);
        break;
    case 10:
        ui.expires->setCurrentIndex(1);
        break;
    case 1440:
        ui.expires->setCurrentIndex(2);
        break;
    case 10080:
        ui.expires->setCurrentIndex(3);
        break;
    case 20160:
        ui.expires->setCurrentIndex(4);
        break;
    case 43200:
        ui.expires->setCurrentIndex(5);
        break;
    default:
        break;
    }

    connect(ui.listDevices, &QPushButton::clicked, control, &Desktop::openDeviceList);
    connect(ui.ExportDb,&QPushButton::clicked,control,&Desktop::exportDb);
    connect(ui.ImportDb,&QPushButton::clicked,control,&Desktop::importDb);
    connect(ui.pickfontbutton, &QPushButton::clicked, this, &Options::fontDialog);
    connect(ui.resetFont, &QPushButton::clicked , control , &Desktop::resetFont);
    connect(ui.confirm, &QLineEdit::textChanged, this, &Options::secretChanged);
    connect(ui.secret, & QLineEdit::textChanged, this, &Options::secretChanged);
    connect(ui.changeSecret, &QPushButton::clicked, this, &Options::changePassword);
    connect(ui.appearance, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), desktop, &Desktop::changeAppearance);

    connect(ui.secret, &QLineEdit::returnPressed, this, []{
        if(!ui.secret->text().isEmpty())
            ui.confirm->setFocus();
    });

    connect(ui.confirm, &QLineEdit::returnPressed, this, [this] {
        if(ui.confirm->text().isEmpty() || ui.confirm->text() != ui.secret->text())
            ui.secret->setFocus();
        else
            changePassword();
    });

    connect(ui.expires,static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), [=](const QString &expiresIndex) {
        int expiration = 86400;
        if (expiresIndex == "3 min")
        {
            expiration = 3 * 60;
            desktop->changeExpiration(expiration);
        } else if (expiresIndex == "10 min")
        {
            expiration = 10 * 60;
            desktop->changeExpiration(expiration);
        } else if (expiresIndex == "1")
        {
            expiration = 1 * 86400;
            desktop->changeExpiration(expiration);
        } else if (expiresIndex == "7")
        {
            expiration = 7 * 86400;
            desktop->changeExpiration(expiration);
        } else if (expiresIndex == "14")
        {
            expiration = 14 * 86400;
            desktop->changeExpiration(expiration);
        }else if (expiresIndex == "30")
        {
            expiration = 30 * 86400;
            desktop->changeExpiration(expiration);
        }

        auto toolbar = Toolbar::instance();
        toolbar->noSearch();
        toolbar->noSession();
    });

    connect(ui.fontSize, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged) , [=](const int ind){

        int change = 0;
        if (ind == 0){ change = 6 ; }
        else if (ind == 1) { change = 8; }
        else if (ind == 2) { change = 10; }
        else if (ind == 3) { change = 13; }
        else if (ind == 4) { change = 15; }
        else if (ind == 5) { change = 18; }


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

    auto creds = storage->credentials();
    ui.confirm->setText("");
    ui.secret->setText("");
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

void Options::secretChanged(const QString& text)
{
    if(ui.secret->text() == ui.confirm->text())
        ui.confirm->setStyleSheet("background: white; padding: 1px;");
    else
        ui.confirm->setStyleSheet("color: red; background: white; padding: 1px");
}

void Options::changePassword()
{
    Desktop *desktop = Desktop::instance();

    if(ui.secret->text() != ui.confirm->text()) {
        desktop->errorMessage(tr("Password doesn't match"));
        return;
    }
    if(ui.secret->text().isEmpty()) {
        desktop->errorMessage(tr("No password entered"));
        return;
    }

    desktop->changePassword(ui.secret->text());
    ui.secret->setText("");
    ui.confirm->setText("");
}
