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

#ifndef __DATABASE_HPP__
#define __DATABASE_HPP__

#include "request.hpp"
#include "sqldriver.hpp"

#include <QObject>
#include <QString>
#include <QDebug>
#include <QSqlDatabase>

class Database final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Database)

public:
    ~Database();

    static Database *instance() {
        return Instance;
    }

    static void init(unsigned order);

    static void countExtensions();

private:
    static const int interval = 10000;

    QSqlDatabase db;
    QSqlRecord config;
    QTimer timer;
    QString uuid;
    QString realm;
    QString driver;
    QString host;
    QString name;
    QString user;
    QString pass;
    int port;

    Database(unsigned order);

    bool event(QEvent *evt) final;

    int getCount(const QString& id);

    bool runQuery(const QString& string, QVariantList parms = QVariantList());
    int runQuery(const QStringList& list);
    QSqlRecord getRecord(const QString& request, QVariantList parms = QVariantList());
    bool reopen();
    bool create();
    void close();

    static Database *Instance;

signals:
    void countResults(const QString& id, int count);

private slots:
    void applyConfig(const QVariantHash& config);
    void onTimeout();
};

/*!
 * A database system for sipwitch.  This can support direct database
 * access and queued requests.
 * \file database.hpp
 */

/*!
 * \class Database
 * \brief SipWitchQt database engine class.
 * The database engine operates in it's own thread context and uses a custom
 * event system.  By using a single and separate thread for all db operations
 * we avoid conflicts and problems with db drivers that are not thread-safe
 * or require complex thread locking schemes.
 *
 * This engine offers both direct query operations that are signaled, and can
 * process special requests objects.  Query/response thru a separate thread
 * allows fully asychronous operations with other services that may have their
 * own thread contexts and event loops, such as the stack manager.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
