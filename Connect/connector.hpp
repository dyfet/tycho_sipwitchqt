#include <climits>/*
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

#ifndef CONNECTOR_HPP_
#define CONNECTOR_HPP_

#include "../Common/compiler.hpp"
#include "../Common/util.hpp"
#include "../Common/contact.hpp"

#include <QThread>
#include <QSslCertificate>
#include <QTimer>

class Connector final : public QObject
{
    Q_DISABLE_COPY(Connector)
	Q_OBJECT

public:
    explicit Connector(const QVariantHash& credentials, const QSslCertificate& cert = QSslCertificate());

    inline void start() {
        thread()->start();
    }

    void stop(bool shutdown = false);
    void requestRoster();
    bool sendText(const UString& to, const UString& body, const UString subject = "None");

private:
    volatile bool active;

    QVariantHash serverCreds;
    UString uriFrom, uriRoute, sipFrom;
    UString serverId;
    UString serverHost;
    UString serverSchema;
    UString serverLabel;
    UString serverDisplay;
    quint16 serverPort;

    eXosip_t *context;
    int family, tls;

    void add_authentication();

    const UString sipTo(const UString& to, const QList<QPair<UString,UString>>args) const;
    void processRoster(eXosip_event_t *event);

signals:
    void starting();
	void finished();
    void failure(int code);

    void messageResult(int status);
    void changeRoster(const QByteArray& json);

private slots:
	void run();
};

/*!
 * Interface to remote sip server.  A key difference between connector and
 * listener is that connector is used to engage longer transactions over tcp.
 * It is also only created once registration completes in the listener.
 * \file connector.hpp
 */

#endif
