/**
 ** Copyright 2017 Tycho Softworks.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "compiler.hpp"
#include "util.hpp"
#include "subnet.hpp"

#include <QThread>
#include <QMutex>
#include <QHostAddress>
#include <QAbstractSocket>
#include <eXosip2/eXosip.h>

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

    typedef struct {
        const char *name;
        const char *schema;
        Protocol proto;
    } Schema;

    QAbstractSocket::NetworkLayerProtocol protocol();

    static const unsigned max = 1<<4;

    Context(const QHostAddress& bind, int port, const QString& name, const Schema& choice);

    inline const QString hostname() {
        return localHosts[0];
    }

    void setOtherNets(QList<Subnet> subnets);

    void setOtherNames(QStringList names);

    const QStringList localnames();

    const QList<Subnet> localnets();

    inline static void enableTrusted() {
        trusted = true;
    }

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
    eXosip_t *context;
    time_t currentEvent, priorEvent;
    int netFamily, netPort, netTLS;
    QByteArray netAddr;
    QStringList localHosts, otherNames;
    QList<Subnet> localSubnets, otherNets;

    static QList<Context::Schema> Schemas;
    static QList<Context *> Contexts;
    static bool trusted;                // trust subnets auth flag

    ~Context();

    void init(void);

    bool process(const eXosip_event_t *ev);

    const char *eid(eXosip_event_type ev);

signals:
    void finished();

private slots:
    void run();
};
