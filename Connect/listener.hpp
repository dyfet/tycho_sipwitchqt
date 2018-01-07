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

#ifndef LISTENER_HPP_
#define LISTENER_HPP_

#include "../Common/compiler.hpp"
#include "../Common/util.hpp"
#include "../Common/contact.hpp"

#include <QThread>
#include <QSslCertificate>

class Listener final : public QObject
{
	Q_DISABLE_COPY(Listener)
	Q_OBJECT

public:
    Listener(const QVariantHash& credentials, const QSslCertificate& cert = QSslCertificate());

    inline void start() {
        thread()->start(QThread::HighPriority);
    }

    void stop();

private:
    volatile bool active;
    UString serverAddress;
    quint16 serverPort;
    eXosip_t *context;
    int family, tls;
    int rid;

    void _listen();

signals:
    void starting();
	void finished();

private slots:
	void run();
};

/*!
 * Interface to remote sip server.  A key difference between listener and
 * the server sip context is that there is only one listener, and it
 * represents a single active authenticated session.  All sip functionality
 * is also enclosed here.
 * \file listener.hpp
 */

#endif