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

#ifndef DATABASE_HPP_
#define DATABASE_HPP_

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
    friend class Authorize;

public:
    ~Database() final;

    bool isFile() const {
        return Util::dbIsFile(dbDriver);
    }

    static Database *instance() {
        return Instance;
    }

    static void init(unsigned order);

    static void countExtensions();

    static QPair<int,int> range() {
        if(!Instance)
            return {0, 0};
        return {static_cast<int>(Instance->firstNumber), static_cast<int>(Instance->lastNumber)};
    }

    static int sequence() {
        if(!Instance)
            return 0;
        return Instance->dbSequence;
    }

    static void updateSequence() {
        if(Instance)
           ++Instance->dbSequence;
    }

private:
    static const int interval = 10000;

    QSqlDatabase db;
    QSqlRecord dbConfig;
    QTimer dbTimer;
    QString dbUuid;
    QString dbRealm;
    QString dbDriver;
    QString dbHost;
    QString dbName;
    QString dbUser;
    QString dbPass;
    int dbPort;

    int expiresNat, expiresUdp, expiresTcp;

    volatile int firstNumber, lastNumber, dbSequence;

    Database(unsigned order);

    bool event(QEvent *evt) final;

    int getCount(const QString& id);

    bool runQuery(const QString& string, const QVariantList &parms = QVariantList());
    int runQueries(const QStringList& list);
    QVariant insert(const QString& request, const QVariantList &parms = QVariantList());
    QSqlRecord getRecord(const QString& request, const QVariantList &parms = QVariantList());
    QSqlQuery getRecords(const QString& request, const QVariantList& parms = QVariantList());
    bool resume();
    bool reopen();
    bool create();
    void close();

    static Database *Instance;

signals:
    void updateAuthorize(const QVariantHash& config, bool active);
    void sendMessage(qlonglong endpoint, const QVariantHash& data);
    void disconnectEndpoint(qlonglong endpoint);

public slots:
    void localMessage(const Event& ev);
    void copyOutbox(qlonglong source, qlonglong target);
    void syncOutbox(qlonglong endpoint);
    void messageResponse(const QByteArray& mid, const QByteArray &ep, int status);

private slots:
    void sendRoster(const Event& ev);
    void sendProfile(const Event& ev, const UString& auth, qlonglong endpoint);
    void sendPending(const Event& ev, qlonglong endpoint);
    void changeAuthorize(const Event& ev);
    void sendDeviceList(const Event& ev);
    void changePending(qlonglong endpoint);
    void lastAccess(qlonglong endpoint, const QDateTime& timestamp);
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
