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

#include "../sys/server.hpp"
#include "../sys/control.hpp"
#include "../sys/logging.hpp"
#include "../sip/stack.hpp"

static QHash<QCryptographicHash::Algorithm,QByteArray> digests = {
    {QCryptographicHash::Md5,       "MD5"},
    {QCryptographicHash::Sha256,    "SHA-256"},
    {QCryptographicHash::Sha512,    "SHA-512"},
};

Stack *Stack::Instance = nullptr;
QString Stack::UserAgent;
QString Stack::ServerRealm;
QStringList Stack::ServerAliases;
QStringList Stack::ServerNames;
QCryptographicHash::Algorithm Stack::Digest = QCryptographicHash::Md5;
unsigned Stack::Contexts = 0;

Stack::Stack(unsigned order)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;

    qRegisterMetaType<Event>("Event");

    moveToThread(Server::createThread("stack", order));
    UserAgent = qApp->applicationName() + "/" + qApp->applicationVersion();
#ifndef Q_OS_WIN
    osip_trace_initialize_syslog(TRACE_LEVEL0, const_cast<char *>(Server::name()));
#endif
}

Stack::~Stack()
{
    Instance = nullptr;
}

const QByteArray Stack::digestName()
{
    return digests.value(Digest, "");
}

const QByteArray Stack::computeDigest(const QString& id, const QString& secret)
{
    if(secret.isEmpty() || id.isEmpty())
        return QByteArray();

    return QCryptographicHash::hash(id.toUtf8() + ":" + realm() + ":" + secret.toUtf8(), Digest);
}

void Stack::create(const QHostAddress& addr, int port, unsigned mask)
{
    unsigned index = ++Contexts;

    qDebug().nospace() << "Creating sip" << index << " " <<  addr << ", port=" << port << ", mask=" << QString("0x%1").arg(mask, 8, 16, QChar('0'));

    foreach(auto schema, Context::schemas()) {
        if(schema.proto & mask) {
            new Context(addr, port, schema, mask, index);
        }
    }
}

void Stack::create(const QList<QHostAddress>& list, int port, unsigned  mask)
{
    foreach(auto host, list) {
        create(host, port, mask);
    }
}

void Stack::registry(const Event &ev)
{
    Registry::events(ev);
}


