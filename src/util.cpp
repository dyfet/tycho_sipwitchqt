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

#include "compiler.hpp"
#include "util.hpp"
#include <QHostInfo>
#include <QNetworkInterface>
#include "system.hpp"

namespace Util {
    const QStringList controlOptions(const char **argv)
    {
        QStringList cmds;
        if(!argv[0] || !argv[1])
            return cmds;

        if(System::isShellAlias(argv[0]))
            ++argv;
#ifdef Q_OS_WIN
        else if(!strcmp(argv[1], "/c"))
            argv += 2;
#endif
        else
            return cmds;

        while(*argv) {
            cmds << *(argv++);
        }
        return cmds;
    }

    const QString localDomain()
    {
        QString domain = QHostInfo::localDomainName();
        if(domain.isEmpty()) {
            QString host = QHostInfo::localHostName();
            int pos = host.indexOf(".");
            if(pos < 0)
                domain = "localdomain";
            else
                domain = host.mid(++pos);
        }
        return domain;
    }

    const QList<QHostAddress> hostAddress(const QString& hostId)
    {
        int pos = hostId.indexOf(":");
        QString bindId = hostId;

        if(pos > 0)
            bindId = hostId.left(pos);

        QList<QHostAddress> list;
        if(bindId != "none") {
            auto iface = QNetworkInterface::interfaceFromName(bindId);
            if(iface.isValid()) {
                auto entries = iface.addressEntries();
                foreach(auto entry, entries) {
                    list << entry.ip();
                }
            }
        }
        return list;
    }

    int hostPort(const QString& hostId)
    {
        int pos = hostId.indexOf(":");
        if(pos < 1)
            return 0;
        pos = hostId.mid(pos).toInt();
        if((pos < 0) || (pos > 65535))
            pos = 0;
        return pos;
    }

    const QList<QHostAddress> bindAddress(const QString& hostId)
    {
        QList<QHostAddress> list;
        if(hostId == "*" || hostId == "all" || hostId == "any") {
            list << QHostAddress(QHostAddress::Any);
        }
        else {
            auto iface = QNetworkInterface::interfaceFromName(hostId);
            if(iface.isValid()) {
                auto entries = iface.addressEntries();
                foreach(auto entry, entries) {
                    list << entry.ip();
                }
            }
            else {
                QHostInfo hostinfo = QHostInfo::fromName(hostId);
                list = hostinfo.addresses();
            }
        }
        return list;
    }

    const QString exePath(const QString& path)
    {
        if(path.startsWith("../"))
            return QCoreApplication::applicationDirPath() + "/" + path;
        if(path.startsWith("./"))
            return QCoreApplication::applicationDirPath() + "/" + path.mid(3);
        if(path == ".")
            return QCoreApplication::applicationDirPath();
        return path;
    }
}
