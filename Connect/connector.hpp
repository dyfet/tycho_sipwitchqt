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

    void requestProfile(const UString& who) {
        sendProfile(who, QByteArray());
    }

    void stop(bool shutdown = false);
    void ackPending();
    void requestRoster();
    void requestPending();
    void requestDeviceList();
    void requestDeauthorize(const UString& to);
    void createAuthorize(const UString& to, const QByteArray& body);
    void changeTopic(const UString& to, const UString& subject, const UString& body = {});
    void changeMemebership(const UString& to, const UString& subject, const UString& members, const UString& admin, const UString& notify, const UString& reason);
    void sendProfile(const UString& to, const QByteArray& body);
    bool sendText(const UString& to, const UString& body, const UString &subject = "None");

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
    void processRoster(eXosip_event_t *event);
    void processProfile(eXosip_event_t *event);
    void processPending(eXosip_event_t *event);
    void processDeviceList(eXosip_event_t *event);
    void processDeauthorize(eXosip_event_t *event);

signals:
    void starting();
	void finished();
    void failure(int code);
    void topicFailed();

    void statusResult(int status, const QString& text);
    void messageResult(int status, const QDateTime& timestamp, int sequence);
    void syncPending(const QByteArray& json);
    void changeRoster(const QByteArray& json);
    void changeProfile(const QByteArray& json);
    void changeDeviceList(const QByteArray& json);
    void deauthorizeUser(const UString& id);

private slots:
	void run();
};

/*!
 * Interface to remote sip server.  A key difference between connector and
 * listener is that connector is used to engage longer transactions over tcp.
 * It is also only created once registration completes in the listener.
 * \file connector.hpp
 */

/*!
 * \class Connector
 * \brief Provides a connection service for query-response operations that
 * may generate longer responses from the server than the real-time Listener
 * can handle.  The connector is used for operations like rosters, profiles,
 * and message syncing.  It will also be used for extended content messages.
 *
 * The Connector operates in it's own detached thread that receives eXosip
 * events and emits server responses as signals.  A context lock is used
 * to support calling methods that invoke server operations from the ui thread
 * context.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
