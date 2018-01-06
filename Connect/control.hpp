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

#ifndef CONTROL_HPP_
#define	CONTROL_HPP_

#include "../Common/types.hpp"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QSettings>
#include <QLocalServer>
#include <QLockFile>
#include <QVariant>
#include <QHash>

class Control : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Control)

public:
    Control(QObject *parent);
    ~Control();

    inline static Control *instance() {
	return Instance;
    }

    static int command(const QString& value, bool verbose = false, int timeout = 1000);

    static int request(const QStringList& args, bool verbose = true, int timeout = 1000);

private:
    QLockFile lockFile;
    QLocalServer localServer;

    static Control *Instance;

signals:
    void request(const QString& msg);

private slots:
    void acceptConnection();
};

/*!
 * Local IPC control support of a sipwitchqt client.
 * \file control.hpp
 */

#endif	
