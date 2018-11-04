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

#include "about.hpp"
#include "ui_about.h"
#include <QUrl>
#include <QSslSocket>
#include <QDesktopServices>

static Ui::About ui;

About::About(Desktop *parent) :
QDialog(parent, Qt::Popup|Qt::WindowTitleHint|Qt::WindowCloseButtonHint)
{
    auto sslVersion = QSslSocket::sslLibraryBuildVersionString();
    if(sslVersion.size() < 1 || !QSslSocket::supportsSsl())
        sslVersion = "No Secure Socket Support Enabled";

    ui.setupUi(static_cast<QDialog*>(this));
    ui.labelVersion->setText(tr("SipWitchQt Version: ") + PROJECT_VERSION);
    ui.labelSsl->setText(sslVersion);
    ui.labelCopyright->setText(tr("Copyright (C) ") + PROJECT_COPYRIGHT + " Tycho Softworks");
    ui.logoButton->setVisible(false);
    setWindowTitle(tr("About SipWitchQt Desktop"));

    connect(ui.aboutButton, &QToolButton::clicked, this, &About::aboutProject);
    connect(ui.tribalButton, &QToolButton::clicked, this, &About::aboutTribal);
    connect(ui.closeButton, &QPushButton::clicked, this, &About::reject);

    auto creds = parent->storageCredentials();
    if(!creds["host"].isNull()) {
        ui.authServer->setText(creds["host"].toString());
        ui.authId->setText(creds["user"].toString());
        ui.authLabel->setText(creds["label"].toString());
    }

    auto deviceKey = creds["devkey"].toByteArray().toHex();
    auto len = deviceKey.count();
    if(len) {
        ui.deviceKey1->setText(deviceKey.left(len/2));
        ui.deviceKey2->setText(deviceKey.mid(len/2));
    }
    show();
}

void About::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    Desktop::instance()->closeDialog();
}

void About::reject()
{
    QDialog::reject();
    Desktop::instance()->closeDialog();
}

void About::aboutProject()
{
    QDesktopServices::openUrl(QUrl("https://gitlab.com/tychosoft/sipwitchqt"));
}

void About::aboutTribal()
{
    QDesktopServices::openUrl(QUrl("https://www.cherokeesofidaho.org"));
}
