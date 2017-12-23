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

#ifndef REQUEST_HPP_
#define REQUEST_HPP_

#include <QVariant>
#include <QCoreApplication>
#include <QList>
#include <QSqlRecord>
#include <QSqlQuery>
#include <QTimer>

#include "../Server/event.hpp"

class Request : public QObject
{
	Q_OBJECT
	Q_DISABLE_COPY(Request)
public:
    typedef enum {
        Success = 0,        // successful query
        Timeout,            // query timed out
        DbFailed,           // database connect failure
        NotFound,           // query failed to select
        Partial,            // partial result
        Invalid,            // query invalid
        Immediate,          // immediate result returned (same as success)
    }   ErrorResult;

    typedef void (QObject::*Reply)(ErrorResult, const Event&, const QList<QSqlRecord>&);

    Request(QObject *parent, const Event& sip, int expires);
    Request(QObject *parent, const Event& sip, int expires, Reply method);

    const Event& event() const {
        return sipEvent;
    }

    inline bool isSignalled() {
        return signalled;
    }

    bool cancelled(void);

    void notifySuccess(QSqlQuery &results, ErrorResult error = Success);
    void notifyFailed(ErrorResult error = DbFailed);
	
private:
    ErrorResult status;
    Event sipEvent;
    volatile bool signalled;

    bool event(QEvent *evt) final;

signals:
    void results(ErrorResult, const Event&, const QList<QSqlRecord>&);

private slots:
	void timeout();
};

/*!
 * Query request objects.
 * \file request.hpp
 */

/*!
 * \class Request
 * \brief The query request class.
 * This class is meant to queue a request to the database engine.  Results
 * are processed in a private event loop.  This is done because the request
 * will be created in the thread context of the originating object, not the
 * database thread.  Hence, results have to be transposed from the database
 * engine thread to the requestors thread (parent qobject), and may even be
 * signalled to a different final object.  This also gaurentees that timeouts
 * are signalled on the correct thread and in a constitent manner, rather
 * than racing.
 *
 * As each request can be unique, a single result signal is emitted.  This
 * is based on the idea that a request can be created with new and connected
 * to it's result handler on demand as needed before being sent to the
 * database engine.  It is also possible to create derived request objects,
 * and I do this in sipwitch to modularize & isolate the queries and db
 * handling code for simpler maintainability.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif	
