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
#include <QDir>
#include <climits>

#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif

static Ui::OptionsWindow ui;
static QLineEdit *speeds[10];
static bool updateDisabled = true;

Options::Options(Desktop *control) :
QWidget(), desktop(control), connector(nullptr)
{
    ui.setupUi(static_cast<QWidget *>(this));
    ui.appearance->setCurrentText(desktop->appearance());

    speeds[1] = ui.speed1;
    speeds[2] = ui.speed2;
    speeds[3] = ui.speed3;
    speeds[4] = ui.speed4;
    speeds[5] = ui.speed5;
    speeds[6] = ui.speed6;
    speeds[7] = ui.speed7;
    speeds[8] = ui.speed8;
    speeds[9] = ui.speed9;

    ui.delayRinging->setEnabled(false);
    ui.creds->setEnabled(false);
    for(auto spd = 1; spd < 10; ++spd)
        speeds[spd]->setEnabled(false);

#if defined(Q_OS_WIN)
    // set with "APPLICATION_NAME" and value as Argv0 path
    autostartPath = "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
    QSettings reg(autostartPath, QSettings::NativeFormat);
    auto execPath = reg.value(APPLICATION_NAME).toString();
    qDebug() << "STRING FOR" << execPath << "OF" << APPLICATION_NAME;
    if(!execPath.isEmpty())
        ui.startAtLogin->setValue(1);
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    autostartPath = QDir::homePath() + "/.config/autostart/" + APPLICATION_EXEC + ".desktop";
    if(QFile::exists(autostartPath))
        ui.startAtLogin->setValue(1);
#endif

    auto expires = desktop->expiration();
    if(expires == 0 || expires == INT_MAX)
        ui.expires->setCurrentIndex(4);
    else {
        expires /= 86400;
        if(expires <= 7)
            ui.expires->setCurrentIndex(0);
        else if(expires <= 14)
            ui.expires->setCurrentIndex(1);
        else if(expires <= 30)
            ui.expires->setCurrentIndex(2);
        else if(expires <= 90)
            ui.expires->setCurrentIndex(3);
        else
            ui.expires->setCurrentIndex(4);
    }

    auto base = QGuiApplication::font().pointSize();
    auto points = desktop->getCurrentFont().pointSize();
    auto family = desktop->getCurrentFont().family();
    if(family[0] == '.')
        family = desktop->getCurrentFont().lastResortFamily();
    if(points < base - 5)
        ui.fontSize->setCurrentIndex(0);
    else if(points < base - 2)
        ui.fontSize->setCurrentIndex(1);
    else if(points < base + 2)
        ui.fontSize->setCurrentIndex(2);
    else if(points < base + 4)
        ui.fontSize->setCurrentIndex(3);
    else if(points < base + 6)
        ui.fontSize->setCurrentIndex(4);
    else
        ui.fontSize->setCurrentIndex(5);

    ui.fontPicker->setCurrentFont(QFont(family, points));

    if(desktop->isAutoAnswer())
        ui.autoAnswer->setValue(1);

    if(desktop->isDesktopNotify())
        ui.desktopNotify->setValue(1);

    if(desktop->isAudioNotify())
        ui.audioNotify->setValue(1);

    connect(ui.listDevices, &QPushButton::clicked, control, &Desktop::openDeviceList);
    connect(ui.ExportDb,&QPushButton::clicked,control,&Desktop::exportDb);
    connect(ui.ImportDb,&QPushButton::clicked,control,&Desktop::importDb);
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
        auto expiresDays = expiresIndex.toInt();
        if(expiresDays < 1)
            desktop->changeExpiration(INT_MAX);
        else
            desktop->changeExpiration(expiresDays * 86400);

        auto toolbar = Toolbar::instance();
        toolbar->noSearch();
        toolbar->noSession();
    });

    connect(ui.autoAnswer, &QSlider::valueChanged, this, [=](int value) {
        if(value)
            desktop->enableAutoAnswer();
        else
            desktop->disableAutoAnswer();
    });

    connect(ui.desktopNotify, &QSlider::valueChanged, this, [=](int value) {
        if(value)
            desktop->enableDesktopNotify();
        else
            desktop->disableDesktopNotify();
    });

    connect(ui.audioNotify, &QSlider::valueChanged, this, [=](int value) {
        if(value)
            desktop->enableAudioNotify();
        else
            desktop->disableAudioNotify();
    });

    connect(ui.fontPicker, &QFontComboBox::currentFontChanged, this, [=](const QFont& font) {
        control->setTheFont(font);
    });

    connect(ui.delayRinging, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged) , [=](int priority) {
        if(updateDisabled)
            return;

        if(connector) {
            desktop->statusMessage(tr("changing ringing"));
            connector->changeCoverage(Phonebook::self()->uri(),--priority);
        }
        else
            desktop->errorMessage(tr("Cannot modify while offline"));
    });

    connect(ui.fontSize, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged) , [=](const int ind) {
        int change = QGuiApplication::font().pointSize();   // system base size...
        switch(ind) {
        case 0:
            change -= 6;
            break;
        case 1:
            change -= 3;
            break;
        case 2:
            break;
        case 3:
            change += 2;
            break;
        case 4:
            change += 4;
            break;
        case 5:
            change += 6;
            break;
        }

        auto font = desktop->getCurrentFont();
        font.setPointSize(change);
        desktop->setTheFont(font);
    });

    connect(desktop, &Desktop::changeConnector, this, [=](Connector *connection) {
        connector = connection;
        if(connector) {
            ui.delayRinging->setEnabled(true);
            ui.creds->setEnabled(true);
            for(auto spd = 1; spd < 10; ++spd)
                speeds[spd]->setEnabled(true);
        }
        else {
            ui.delayRinging->setEnabled(false);
            ui.creds->setEnabled(false);
            for(auto spd = 1; spd < 10; ++spd)
                speeds[spd]->setEnabled(false);
        }
    });

    connect(ui.startAtLogin, &QSlider::valueChanged, this, &Options::changeAutostart);
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

    auto self = Phonebook::self();
    if(!self)
        return;

    updateDisabled = true;
    for(int key = 1; key < 10; ++key)
        speeds[key]->setText(self->speed(key));

    ui.delayRinging->setCurrentIndex(self->delay() + 1);
    updateDisabled = false;
}

void Options::secretChanged(const QString& text)
{
    Q_UNUSED(text);
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

#if defined(Q_OS_WIN)
void Options::changeAutostart(int value)
{
    auto execPath = QCoreApplication::applicationFilePath();
    QSettings reg(autostartPath, QSettings::NativeFormat);
    if(value)
        reg.setValue(APPLICATION_NAME, execPath);
    else
        reg.remove(APPLICATION_NAME);
}
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
void Options::changeAutostart(int value)
{
    if(value) {
        QFile file(autostartPath);
        if(!file.open(QIODevice::WriteOnly)) {
            desktop->errorMessage(tr("Cannot create autologin file"), 3000);
            ui.startAtLogin->setValue(0);
            return;
        }
        QTextStream out(&file);
        out << "[Desktop Entry]\n"
            << "Name=" APPLICATION_NAME "\n"
            << "Exec=" APPLICATION_EXEC " -minimize\n"
            << "Terminal=false\n"
            << "Icon=" APPLICATION_EXEC "\n"
            << "Categories=Network\n"
            << "Type=Application\n"
            << "StartupNotify=false\n"
            << "X-GNOME-Autostart-enabled=true\n";
    }
    else
        QFile::remove(autostartPath);
}
#else
void Options::changeAutostart(int value)
{
    Q_UNUSED(value);
    ui.startAtLogin->setValue(0);
    desktop->errorMessage(tr("Autostart not supported on this platform"), 3000);
}
#endif
