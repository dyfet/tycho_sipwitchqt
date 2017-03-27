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

#include "server.hpp"
#include "control.hpp"
#include "logging.hpp"
#include "stack.hpp"

Stack *Stack::Instance = nullptr;
QString Stack::UserAgent;
QString Stack::ServerRealm;
QStringList Stack::ServerAliases;
QStringList Stack::ServerNames;
QCryptographicHash::Algorithm Stack::Digest = QCryptographicHash::Md5;

Stack::Stack(unsigned order)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;

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
    switch(Digest) {
    case QCryptographicHash::Md5:
	return "MD5";
    case QCryptographicHash::Sha256:
	return "SHA-256";
    case QCryptographicHash::Sha512:
	return "SHA-512";
    default:
	return "";
    }
}

const QByteArray Stack::computeDigest(const QString& id, const QString& secret)
{
    if(secret.isEmpty() || id.isEmpty())
	return QByteArray();

    QCryptographicHash hash(digestAlgorithm());
    hash.addData(id.toUtf8() + ":" + realm() + ":" + secret.toUtf8());
    return hash.result();
}

