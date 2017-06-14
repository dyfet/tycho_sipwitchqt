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
#include <QSqlRecord>
#include <QList>

class RemoteSegment;      // call segment

class Provider final
{
    Q_DISABLE_COPY(Provider)

public:
    Provider(const QSqlRecord& db);
    ~Provider();

    inline const QSqlRecord data() const {
        return provider;
    }

    static Provider *find(const QString& target);

private:
    const QSqlRecord provider;
    const QString uri;

    QList<RemoteSegment*> calls;         // calls on this provider
};

QDebug operator<<(QDebug dbg, const Provider& prov);
