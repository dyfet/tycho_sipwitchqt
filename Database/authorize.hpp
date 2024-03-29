/*
 * Copyright (C) 2017-2018 Tycho Softworks.
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

#ifndef AUTHORIZE_HPP_
#define AUTHORIZE_HPP_

#include "database.hpp"

class Authorize : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Authorize)
    friend class Database;

public:
    ~Authorize() override;

    static Authorize *instance() {
        return Instance;
    }

    static void init(unsigned order);

protected:
    explicit Authorize(unsigned order);

    bool checkConnection();
    bool resume();
    bool runQuery(const QString& string, const QVariantList &parms = QVariantList());
    QSqlRecord getRecord(const QString& request, const QVariantList &parms = QVariantList());
    QSqlQuery getRecords(const QString& request, const QVariantList &parms = QVariantList());

private:
    Database *database;
    QSqlDatabase *db;
    QSqlDatabase local;
    bool failed;

    static Authorize *Instance;

signals:
    void createEndpoint(const Event& event, const QVariantHash& endpoint);
    void copyOutboxes(qlonglong source, qlonglong target);
    void syncOutbox(qlonglong endpoint);

protected slots:
    virtual void activate(const QVariantHash& config, bool isOpen);
    virtual void findEndpoint(const Event& event);

private slots:
    void removeAuthorization(const Event& ev);
};

/*!
 * An authorize base class for sipwitch.  This can support direct database access in
 * a separate priority thread, or adapted to use ldap, single threaded sql access, or
 * other means in derived classes.  
 * \file authorize.hpp
 */

/*!
 * \class Authorize
 * \brief SipWitchQt authorize class.
 * This represents a base class shell to handle authorization requests.  These often
 * are more time sensitive and cannot be delayed by long sql queries.  Hence they
 * may be connected to a separate sql query instance in the delegated subclass.  This
 * also allows separation of authorization handling, so ldap or other means can be
 * added in as well.  This base class will hold the signal-slot handling for
 * authorization requests, and slots will be implemented as protectd virtuals.
 */

#endif
