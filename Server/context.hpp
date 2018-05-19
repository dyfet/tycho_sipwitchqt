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

#ifndef CONTEXT_HPP_
#define CONTEXT_HPP_

#include "event.hpp"
#include <QSqlRecord>
#include <QJsonDocument>

class Registry;

class Context final : public QObject
{
    Q_DISABLE_COPY(Context)
    Q_OBJECT
public:
    enum Protocol : unsigned {
        UDP = 1<<0,
        TCP = 1<<1,
        TLS = 1<<2,
        DTLS = 1<<3,
    };

    // permissions to pre-filter sip messages
    enum Allow : unsigned {
        REGISTRY =          1 << 8,     // permit registrations from context
        REMOTE =            1 << 9,     // permit remote operations...
        UNAUTHENTICATED =   1 << 10,    // unathenticated requests allowed
    };

    using Schema = struct {
        UString name;
        UString uri;
        Protocol proto;
        quint16 inPort;
        int inProto;
    };

    QAbstractSocket::NetworkLayerProtocol protocol();

    Context(const QHostAddress& bind, quint16 port, const Schema& choice, unsigned mask, unsigned index = 1);

    const Contact contact(const UString& username = UString()) const {
        return Contact(uriHost, netPort, username);
    }

    inline const UString type() const {
        return schema.name;
    }

    inline const UString prefix() const {
        return schema.uri;
    }

    inline quint16 port() const {
        return netPort;
    }

    inline quint16 defaultPort() const {
        return schema.inPort;
    }

    inline bool isLocal(const UString& host) const {
        if(localHosts.contains(host))
            return true;
        return localnames().contains(host);
    }

    inline static const QList<Context::Schema> schemas() {
        return Schemas;
    }

    inline static const QList<Context *> contexts() {
        return Contexts;
    }

    inline static bool isActive() {
        return instanceCount > 0;
    }

    const UString uriTo(const UString& id, const QList<QPair<UString, UString> > &args = QList<QPair<UString,UString>>()) const;
    const UString uriFrom(const UString& id = UString()) const;
    const UString hostname() const;
    void applyHostnames(const QStringList& names, const QString& host);
    const UString uriTo(const Contact& address) const;
    bool message(const UString& from, const UString& to, const UString& route, QList<QPair<UString, UString> > &headers, const UString &type, const QByteArray &body);

    static void challenge(const Event& event, Registry *registry, bool reuse = false);
    static bool answerWithJson(const Event& event, const QByteArray& json);
    static bool reply(const Event& event, int code);
    static bool answerWithTimestamp(const Event& event, int code = SIP_OK);
    static bool authorize(const Event& event, const Registry* registry, const UString &xdp);
    static void start(QThread::Priority priority = QThread::InheritPriority);
    static void shutdown();
    static void dump(const osip_message_t *msg);

private:
    ~Context() final;

    const Schema schema;
    unsigned allow;
    eXosip_t *context;
    time_t currentEvent, priorEvent;
    int netFamily, netTLS, netProto;
    quint16 netPort;
    UString netAddress, uriAddress, uriHost, publicName;
    QStringList localHosts, otherNames;
    mutable QMutex nameLock;
    bool multiInterface;

    const QStringList localnames() const;
    bool process(const Event& ev);
    void messageResponse(const Event& ev);

    static volatile unsigned instanceCount;
    static QList<Context::Schema> Schemas;
    static QList<Context *> Contexts;

signals:
    void REQUEST_REGISTER(const Event& ev);
    void REQUEST_OPTIONS(const Event& ev);
    void REQUEST_ROSTER(const Event& ev);
    void REQUEST_DEVLIST(const Event& ev);
    void REQUEST_PENDING(const Event& ev);
    void REQUEST_PROFILE(const Event& ev);
    void REQUEST_TOPIC(const Event& ev);
    void REQUEST_AUTHORIZE(const Event& ev);
    void REQUEST_MEMBERSHIP(const Event& ev);
    void REQUEST_DEAUTHORIZE(const Event& ev);
    void ACK_PENDING(const Event& ev);
    void SEND_MESSAGE(const Event& ev);
    void CALL_INVITE(const Event& ev);
    void LOCAL_MESSAGE(const Event& ev);
    void OUTBOX_MESSAGE(const Event& ev);
    void OUTBOUND_MESSAGE(const Event& ev);
    void MESSAGE_RESPONSE(const QByteArray& mid, const QByteArray& endpoint, int status);
    void finished();

private slots:
    void run();
};

/*!
 * Manage exosip context threads.  These threads can emit signals back
 * to the stack, but cannot receive signals as they run inside an exosip
 * event loop.
 * \file context.hpp
 */

/*!
 * \class Context
 * \brief An exosip2 event context.
 * Each context represents a seperately bound context instance of the
 * exosip2 stack.  These instances are grouped under indexes, and by
 * default the first index is often presumed to be the gateway contexts for
 * connecting with external sip providers and p2p calls.
 *
 * Each context runs it's own exosip2 event thread.  These threads then
 * will signal events back to the stack, thereby serializing requests under
 * the stack's own thread context.  All low level access to exosip2 functions
 * will occur thru context member functions, as Context also supports eXosip
 * locking internally.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
