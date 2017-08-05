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

#include "../Main/manager.hpp"
#include "../Common/logging.hpp"

#define TIMEOUT 10000       // 10 seconds because we can be lazy on these...

ProviderQuery::ProviderQuery() :
Request(Manager::instance(), QVariantHash(), TIMEOUT)
{
    connect(this, &Request::results, this, &ProviderQuery::reload);
}

ProviderQuery::ProviderQuery(const QString &contact) :
Request(Manager::instance(), {{"contact", contact}}, TIMEOUT)
{
    connect(this, &Request::results, this, &ProviderQuery::update);
}

void ProviderQuery::reload(ErrorResult err, const QVariantHash& keys, const QList<QSqlRecord>& records)
{
    Q_UNUSED(keys);

    switch(err) {
    case Request::Success:
        Provider::reload(records);
        break;
    default:
        Logging::err() << "Provider reload failed; error=" << err;
        break;
    }

}

void ProviderQuery::update(ErrorResult err, const QVariantHash& keys, const QList<QSqlRecord>& records)
{
    QString contact = keys.value("contact", "invalid").toString();
    switch(err) {
    case Request::Success:
        if(records.count() < 1) {
            Logging::err() << "Provider " << contact << " does not exist";
            break;
        }
        Provider::update(records[0]);
        break;
    default:
        Logging::err() << "Provider update failed; contact=" << contact << ", error=" << err;
        break;
    }
}
