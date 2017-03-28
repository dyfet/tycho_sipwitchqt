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

#include "stack.hpp"
#include "database.hpp"

class Manager : public Stack
{
    Q_DISABLE_COPY(Manager)

public:
    static void init(unsigned order);

    static void remoteContext(const QHostAddress& addr, int port, unsigned mask);
    static void localContexts(const QList<QHostAddress>& list, int port, unsigned mask);

private:
    Manager(unsigned order);
    ~Manager();

    void applyNames();

    static QString SystemPassword;
    static QString ServerMode;

    static void localContext(const QString& name, const QHostAddress& addr, int port, unsigned mask);

private slots:
    void applyValue(const QString& id, const QVariant& value);
    void applyConfig(const QVariantHash& config);

#ifndef QT_NO_DEBUG
    void reportCounts(const QString& id, int count);
#endif
};
