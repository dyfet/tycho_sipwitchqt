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

#ifndef LISTENER_HPP_
#define LISTENER_HPP_

#include "../Common/compiler.hpp"
#include "../Common/util.hpp"
#include "../Common/contact.hpp"

#include <QThread>
#include <QSslCertificate>
#include <QTimer>

#define AGENT_ALLOWS "INVITE,ACK,OPTIONS,BYE,CANCEL,SUBSCRIBE,NOTIFY,REFER,MESSAGE,INFO,PING"
#define AGENT_EXPIRES 1800

class Listener final : public QObject
{
    Q_DISABLE_COPY(Listener)
    Q_OBJECT
public:
    explicit Listener(const QVariantHash& credentials, const QSslCertificate& cert = QSslCertificate());

    inline void start() {
        thread()->start(QThread::HighPriority);
    }

    void stop(bool shutdown = false);

private:
    volatile bool active, connected, registered, authenticated;

    void send_registration(osip_message_t *msg, bool auth = false);
    bool get_authentication(eXosip_event_t *event);
    void add_authentication();
    bool isLocal(osip_uri_t *uri);
    void receiveMessage(eXosip_event_t *event);
    QVariantHash parseXdp(const UString& text);

    QByteArray priorBanner;
    QByteArray priorOnline;

    QVariantHash serverCreds;
    UString uriFrom, uriRoute, sipLocal, sipFrom;
    UString serverInit;
    UString serverId;
    UString serverHost;
    UString serverSchema;
    UString serverLabel;
    UString serverUser;
    quint16 serverPort;

    int serverFirst, serverLast;

    eXosip_t *context;
    int family, tls;
    int rid;

signals:
    void statusResult(int status, const QString& text);
    void changeStatus(const QByteArray& bitmap, int first, int last);
    void changeBanner(const QString& banner);
    void messageResult(int status);
    void receiveText(const UString& from, const UString& to, const UString& text, const QDateTime& timestamp, int sequence, const UString& subject, const UString& type);
    void authorize(const QVariantHash& creds);
    void updateRoster();
    void failure(int code);
    void starting();
	void finished();

private slots:
    void timeout();
	void run();
};

/*!
 * Interface to remote sip server.  A key difference between listener and
 * the server sip context is that there is only one listener, and it
 * represents a single active authenticated session.  All sip functionality
 * is also enclosed here.
 * \file listener.hpp
 */

/*!
 * \class Listener
 * \brief Provides client real-time sip connection to your SipWitchQt server.
 * This is created to form a connection and deleted to terminate it.  The
 * Listener is typically used for server registration, realtime short messages,
 * to manage call sessions, and for realtime server status notifications.
 *
 * The Listener operates in it's own detached thread that receives eXosip
 * events and emits processed requests as signals.  A context lock is used
 * to support calling methods that invoke server operations from the ui thread
 * context.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
