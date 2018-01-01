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

#include "control.hpp"

#include <QDebug>
#include <QLocalSocket>
#include <QDataStream>
#include <QStandardPaths>

Control *Control::Instance = nullptr;

static QString runtimePath(const QString& fileName)
{
    auto path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#ifdef Q_OS_UNIX
    if(path.isEmpty())
        return "/tmp/." + fileName;
#else
    if(path.isEmpty())
        return fileName;
#endif
    return path + "/" + fileName;
}

Control::Control(QObject *parent) :
QObject(parent), lockFile(runtimePath("sipwitchqt-lock"))
{
	Q_ASSERT(Instance == nullptr);
	Instance = this;

    qRegisterMetaType<UString>("UString");

    if(!lockFile.tryLock(1000)) {
        qWarning() << "Another instance running.";
        ::exit(91);
    }

    localServer.setSocketOptions(QLocalServer::GroupAccessOption | QLocalServer::UserAccessOption);
    if(!localServer.listen("sipwitchqt-ipc")) {
        qWarning() << "Cannot create control.";
		::exit(90);
	}

    connect(&localServer, &QLocalServer::newConnection, this, &Control::acceptConnection);
}

Control::~Control()
{
    if(lockFile.isLocked())
        lockFile.unlock();
}

void Control::acceptConnection()
{
    QLocalSocket* socket = localServer.nextPendingConnection();
    if (!socket)
        return;

    connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    while (socket->bytesAvailable() < static_cast<int>(sizeof(quint32))) {
        if(!socket->isValid())
            return;

        socket->waitForReadyRead(1000);
    }
    QDataStream ds(socket);
    QByteArray msg;
    quint32 remaining;
    ds >> remaining;
    int got = 0;
    msg.resize(static_cast<int>(remaining));
    char *cp = msg.data();
    do {
        got = ds.readRawData(cp, static_cast<int>(remaining));
        if(got < 0)
            break;
        remaining -= static_cast<quint32>(got);
        cp += got;
    } while(remaining && got >= 0 && socket->waitForReadyRead(2000));
    if(got < 0) {
        qWarning() << "control receive failed";
        socket->deleteLater();
        return;
    }
    emit request(msg.constData());
}


