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
#include <QJsonArray>

class Storage final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Storage)

public:
    explicit Storage(const QString &dbname, const QString &key, const QVariantHash& cred = QVariantHash());
    ~Storage() final;

    bool isActive() {
        return db.isValid() && db.isOpen();
    }

    bool isExisting() {
        return existing;
    }

    QSqlDatabase database() {
        return db;
    }

    // if there is more than one row, that is an error...
    QVariantHash credentials() {
        return getRecord("SELECT * FROM credentials");
    }

    void updateCredentials(const QVariantHash &cred);
    void updateSelf(const QString& text);
    void verifyDevices(QJsonArray &devices);
    void verifyDevice(const QString& label, const QByteArray& key, const QString &agent);

    static Storage *instance() {
        return Instance;
    }

    static UString server() {
        return ServerAddress;
    }

    static UString from() {
        return FromAddress;
    }

    static QString name(const QVariantHash& creds, const UString& id);
    static bool exists(const QString &dbName);
    static void remove(const QString &dbName);
    static QVariantHash next(QSqlQuery& query);
    static void initialLogin(const QString& dbName, const QString &key, QVariantHash& creds);

    QVariant insert(const QString& string, const QVariantList& parms = QVariantList());
    bool runQuery(const QString& string, const QVariantList& parms = QVariantList());
    int runQueries(const QStringList& list);
    QSqlQuery getRecords(const QString& request, const QVariantList &parms = QVariantList());
    QVariantHash getRecord(const QString &request, const QVariantList &parms = QVariantList());
    int copyDb(const QString& dbName);
    static bool importDb(const QString &dbName, const QVariantHash &creds);

    void updateExpiration(int expires);

private:
    QSqlDatabase db;
    bool existing;
    static UString ServerAddress;
    static UString FromAddress;
    static Storage *Instance;

    void createKeys(QVariantHash& creds);
};

/*!
 * Local sql storage support of a sipwitchqt client.  There are several
 * essential differences between client storage and server database.  One is
 * that the storage is not async coupled, and so executes in the client event
 * thread.  The other is that it is only sqlite3 based.
 * \file storage.hpp
 */

/*!
 * \class Storage
 * \brief Provides persistant data store for SipWitchQt client applications.
 * This provides an interface to a sqlite database that provides all client
 * data.  This includes server credentials, contacts, messages, and call
 * history for the client.  A unique database may be created for each
 * endpoint the client logins in as.  Only one instance of this class should
 * be used at a time.
 * \author David Sugar <tychosoft@gmail.com>
 *
 * \fn Storage::Storage(const QString &dbname, const QString &key, const QVariantHash& creds);
 * Create a new database, or open an existing one, using the name supplied,
 * and store or update existing credentials when doing so.  When opening an
 * existing db, the creds need only contain those you wish to modify. The
 * database schema is created for new databases.  In the future older databases
 * may also be upgraded when opened.
 * \param dbname Name of database file to open.
 * \param key For future use if we replace sqlite with sqlcipher.
 * \param creds Credentials to store, or to update for an existing database.
 *
 * \fn Storage::isActive()
 * Tests if the database driver is active and the db file was opened
 * successfully.
 * \return true if database is active.
 *
 * \fn Storage::isExisting()
 * Check if existing or newly created database.
 * \return true if pre-existing.
 *
 * \fn Storage::credentials()
 * Retrieves the credentials stored in the database.
 * \return QVariantHash of credentials that were saved.
 *
 * \fn Storage::database()
 * Retrieves the underlying database object for direct use.
 * \return QSqlDatabase object the instance of storage uses.
 *
 * \fn Storage::instance()
 * Since Storage is treated as a singleton, this returns the single instance
 * of Storage currently allocated.
 * \return Storage* to singleton or nullptr if none made.
 *
 * \fn Storage::updateCredentials(const QVariantHash& creds)
 * Used to update the credentials stored in the currently opened database.
 * This only updates values that are supplied.  All other credential values
 * are unchanged in storage.
 * \param creds updated values to store in database.
 *
 * \fn Storage::updateSelf(const QString& displayName)
 * Used to update the display name of the active user.
 * \param displayName Display Name to use for currently logged in user.
 *
 * \fn Storage::server()
 * Convenience method to find the SipWitchQt server uri the currently open
 * database is associated with.
 * \return UString of schema:server-host:port.
 *
 * \fn Storage::from()
 * Convenience method to give you the uri of your endpoint's from address
 * based on the stored credentials in the currently open database.
 * \return UString of schema:ext@server:port.
 *
 * \fn Storage::name(const QVariantHash& creds, const UString& seed)
 * Used to compute a hashed name for the database based on the the
 * credentials the database will use, and a seed string for the hash.
 * \param creds Used to for extension and label.
 * \param seed Defined by application.
 * \return hashed database name as a base64.
 *
 * \fn Storage::exists(const QString& dbname)
 * Tests if the given sqlite database currently exists on disk.
 * \param dbName Name of database file to test for.
 * \return true if the database name is found.
 *
 * \fn Storage::remove(const QString& dbname)
 * \param dbname Name of database file to remove.
 * Removes the given named sqlite database.
 *
 * \fn Storage::initialLogin(const QString& dbname, const QString& key, QVariantHash& creds)
 * \param dbname Name of database file to check.
 * \param key Cipher key for storage.
 * \param creds User credentials to update.
 * This is used to initialize credentials with a valid device key before first login.  If
 * the database already exists then the databases keys and initialization mode are set.
 * Otherwise a new device key is created for use when the device does successfully login.
 *
 * \fn Storage::getRecord(const QString& query, const QVariantList& params)
 * Used to retrieve a single record based on a query string provided.  The
 * query string can use ? for values that will be bound from an optional
 * list of params.  The query must be one that can return a single record.
 * \param query SQL query string, with ? for binding values.
 * \param params to bind ? values with.
 * \return QVariantHash of column values keyed by field name.
 *
 * \fn Storage::getRecords(const QString& query, const QVariantList& params)
 * Used to initiate retrieval of multiple rows based on a query string
 * provided.  The query string can use ? for values that will be bound from
 * an optional list of params.  The next() method can be used to convert
 * each row retrieved into a QVariantHash like getRecord() does.
 * \param query SQL query string, with ? for binding values.
 * \param params to bind ? values with.
 * \return QSqlQuery object to retrieve records from.
 *
 * \fn Storage::next(QSqlQuery& query)
 * A convenience method to be used with getRecords() to retrieve the next row
 * of a query and convert it to a QVariantHash.
 * \param SQL query object currently being processed.
 * \return QVariantHash of column values keyed by field name.
 *
 * \fn Storage::insert(const QString& query, const QVariantList& params)
 * This is used to perform a Sql insert operation as a query string.  The
 * query string can use ? for values that will be bound from an optional list
 * of parameters.  The operation returns a representation of the Sql lastId()
 * method on success.  If your table uses an auto-inc primary key, this will
 * typically be the id of the row inserted which may be needed as a foreign
 * key for constructing joined tables.  For sqlite and mysql this is typically
 * a qlonglong value, for Postgresql this may be an oid.
 * \param query SQL insert query, values with ? values to bind.
 * \param params for ? values to bind.
 * \return QVariant of Sql lastId() function.
 *
 * \fn Storage::runQuery(const QString& string, const QVariantList& params)
 * A convenience method to run a query on the current storage database where
 * we do not care about the results, only if it was successful.  The query
 * string can use ? for values that will be bound from an optional list of
 * params.  The query can be an insert, update, remove, etc.  Generally
 * selects would be done with getRecord(s).
 * \return true if successful.
 *
 * \fn Storage::runQuery(const QStringList& list)
 * A convenience method to run a series of queries that have no substitution
 * values.  This is typically used to load a schema.
 * \param list stand-alone SQL queries to process.
 * \return count of succesful queries.
 *
 * \fn Storage::copyDb(const QString& dbname)
 * Used to create a backup copy of an existing database.
 * \param dbname Name of database to copy.
 * \return 1 on success.
 *
 * \fn Storage::importDb(const QString& dbname)
 * Used to import a backup into the existing database.
 * \param dbname Name of database to import to.
 * \return 1 on success.
 */

#endif	
