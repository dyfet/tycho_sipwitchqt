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
    explicit Control(QObject *parent);
    ~Control() override;

    inline static Control *instance() {
        return Instance;
    }

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

/*!
 * \class Control
 * \brief Provides a means for other applications to send requests to the
 * currently running SipWitchQt client.  In this sense it is a simple IPC
 * service.
 * \author David Sugar <tychosoft@gmail.com>
 *
 * \fn Control::Control(QObject *parent)
 * Creates a single instance of the control interface, bound as a local
 * socket or pipe.  This also maintains a lock file to make sure there is
 * only one instance of the client running.
 * \param parent Pointer to instance of main application object.
 *
 * \fn Control::instance()
 * Provides pointer to the control object if needed to hook up a slot
 * handler for the request() signal.
 * \return single instance of Control that was created.
 *
 * \fn Control::request(const QString& msg)
 * Signals receipt of an IPC request.  This will be parsed and defined by
 * the receiving slot.
 * \param msg String of control text sent to our application.
 */

#endif	
