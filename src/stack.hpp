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
#include "context.hpp"
#include <QMutex>
#include <QCryptographicHash>

class Stack : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Stack)

public:
    inline static Stack *instance() {
        Q_ASSERT(Instance != nullptr);
        return Instance;
    }

    inline static const QByteArray agent() {
        return UserAgent.toUtf8();
    }

    inline static const QByteArray realm() {
        return ServerRealm.toUtf8();
    }

    inline static QCryptographicHash::Algorithm digestAlgorithm() {
        return Digest;
    }

    static const QByteArray digestName();
    static const QByteArray computeDigest(const QString& id, const QString& secret);
    static void create(const QList<QHostAddress>& list, int port, unsigned mask);
    static void create(const QHostAddress& addr, int port, unsigned mask);

protected:
    static QStringList ServerAliases, ServerNames;
    static QString ServerRealm;
    static QString UserAgent;
    static QCryptographicHash::Algorithm Digest;
    static Stack *Instance;
    static unsigned Contexts;
    static QThread::Priority Priority;

    Stack(unsigned order = 0);
    ~Stack();

signals:
    void changeRealm(const QString& realm, const QString& digest);
};
