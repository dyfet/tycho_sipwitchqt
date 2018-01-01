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
	static Listener *instance() {
		return Instance;
	}

    static void start(const UString& address, quint16 port = 5060);
	static void start(const QSslCertificate& cert);
	static void stop();

private:
    bool active;
    UString serverAddress;
    quint16 serverPort;
    eXosip_t *context;
    int family, tls;

    Listener(const UString& address, quint16 port);
	Listener(const QSslCertificate& cert);

    void threading();

    static Listener *Instance;

signals:
	void finished();

private slots:
	void run();
};

/*!
 * Interface to remote sip server.
 * \file listener.hpp
 */

#endif
