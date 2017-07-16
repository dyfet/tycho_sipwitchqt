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

#include "event.hpp"

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

    typedef struct {
        const char *name;
        const char *uri;
        Protocol proto;
        quint16 inPort;
        int inProto;
    } Schema;

    QAbstractSocket::NetworkLayerProtocol protocol();

    static const unsigned max = 1<<4;

    Context(const QHostAddress& bind, int port, const Schema& choice, unsigned mask, unsigned index = 1);

    inline const QString hostname() const {
        if(localHosts.count() < 1)
            return QHostInfo::localHostName();
        return localHosts[0];
    }

    const Contact contact(const QString& username = "") const {
        return Contact(uriHost, netPort, username);
    }

    inline const QString uriFrom(const QString& id = "") const {
        if(id.length() == 0)
            return schema.uri + uriAddress;
        return schema.uri + id + "@" + uriAddress;
    }

    inline const QString type() const {
        return schema.name;
    }

    inline quint16 defaultPort() const {
        return schema.inPort;
    }

    inline bool isLocal(const QString& host) const {
        return localnames().contains(host);
    }

    void setOtherNames(QStringList names);
    void setPublicName(QString name);

    const QStringList localnames() const;

    const QString uriTo(const Contact& address) const;

    inline static const QList<Context::Schema> schemas() {
        return Schemas;
    }

    inline static const QList<Context *> contexts() {
        return Contexts;
    }

    static void start(QThread::Priority priority = QThread::InheritPriority);

    static void shutdown();

private:
    const Schema schema;
    unsigned allow;
    eXosip_t *context;
    time_t currentEvent, priorEvent;
    int netFamily, netPort, netTLS, netProto;
    QByteArray netAddress;
    QString uriAddress, uriHost;
    QString publicName;
    QStringList localHosts, otherNames;
    mutable QMutex nameLock;

    static QList<Context::Schema> Schemas;
    static QList<Context *> Contexts;

    ~Context();

    bool process(const Event& ev);
    bool authenticated(const Event& ev);

signals:
    void registry(const Event& ev);
    void finished();

private slots:
    void run();
};

/*!
 * Manage exosip context threads.  These threads can emit signals back
 * to the stack, but cannot receive signals as they run inside an exosip
 * event loop.
 * \file context.hpp
 * \ingroup Stack
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
 * \ingroup Stack
 */
