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

#ifndef STORAGE_HPP_
#define	STORAGE_HPP_

#include "../Common/types.hpp"
#include <QObject>
#include <QString>
#include <QHash>
#include <QVariantHash>
#include <QSqlDatabase>
#include <QSqlRecord>
#include <QSqlQuery>

class Storage final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Storage)

public:
    explicit Storage(const QString &key = QString(), const QVariantHash& cred = QVariantHash());
    ~Storage() final;

    bool isActive() {
        return db.isValid() && db.isOpen();
    }

    QSqlDatabase database() {
        return db;
    }

    // if there is more than one row, that is an error...
    QVariantHash credentials() {
        return getRecord("SELECT * FROM credentials");
    }

    void updateCredentials(const QVariantHash &cred);

    static Storage *instance() {
        return Instance;
    }

    static UString server() {
        return ServerAddress;
    }

    static UString from() {
        return FromAddress;
    }

    static bool exists();
    static void remove();
    static QVariantHash next(QSqlQuery& query);

    QVariant insert(const QString& string, const QVariantList& parms = QVariantList());
    bool runQuery(const QString& string, const QVariantList& parms = QVariantList());
    int runQuery(const QStringList& list);
    QSqlQuery getRecords(const QString& request, const QVariantList &parms = QVariantList());
    QVariantHash getRecord(const QString &request, const QVariantList &parms = QVariantList());
    QSqlQuery getQuery(const QString& request, const QVariantList& parms = QVariantList());
    int copyDb(void);
    int importDb(void);

private:
    QSqlDatabase db;
    static UString ServerAddress;
    static UString FromAddress;
    static Storage *Instance;
};

/*!
 * Local sql storage support of a sipwitchqt client.  There are several
 * essential differences between client storage and server database.  One is
 * that the storage is not async coupled, and so executes in the client event
 * thread.  The other is that it is only sqlite3 based.
 * \file storage.hpp
 */

#endif	
