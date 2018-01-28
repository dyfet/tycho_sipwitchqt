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

#ifndef AUTHORIZE_HPP_
#define AUTHORIZE_HPP_

#include "database.hpp"

class Authorize : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Authorize)
    friend class Database;

public:
    virtual ~Authorize();

    static Authorize *instance() {
        return Instance;
    }

    static void init(unsigned order);

protected:
    Authorize(unsigned order);

    bool runQuery(const QString& string, const QVariantList &parms = QVariantList());
    int runQuery(const QStringList& list);
    QSqlRecord getRecord(const QString& request, const QVariantList &parms = QVariantList());

private:
    Database *database;
    QSqlDatabase *db;
    QSqlDatabase local;

    static Authorize *Instance;

protected slots:
    virtual void activate(const QVariantHash& config, bool isOpen);
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
