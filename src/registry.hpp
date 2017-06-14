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
#include "endpoint.hpp"
#include <QSqlRecord>

class Registry final
{
    Q_DISABLE_COPY(Registry)

public:
    Registry(const QSqlRecord& db);
    ~Registry();

    inline const QSqlRecord data() const {
        return extension;
    }

    static const Registry *find(const QString& target);

private:
    const QSqlRecord extension;
    const QString id, alias;
    QList<const Endpoint*> endpoints;
};

QDebug operator<<(QDebug dbg, const Registry& registry);
