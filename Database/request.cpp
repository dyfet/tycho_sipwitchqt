#include <utility>

#include <utility>

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

#include "../Common/compiler.hpp"
#include "request.hpp"

#include <QEvent>

namespace {
enum {
    REQUEST_SUCCESS = QEvent::User + 1,
    REQUEST_FAILED
};

class RequestEvent final : public QEvent
{
    Q_DISABLE_COPY(RequestEvent)
public:
    RequestEvent(int event, Request::ErrorResult error = Request::Success, QList<QSqlRecord> results = QList<QSqlRecord>()) :
    QEvent(static_cast<QEvent::Type>(event)), Results(std::move(results)), Error(error) {}

    ~RequestEvent() final;

    const QList<QSqlRecord> results() const {
        return Results;
    } 

    Request::ErrorResult error() const {
        return Error;
    }

private:
    const QList<QSqlRecord> Results;
    Request::ErrorResult Error;
};

RequestEvent::~RequestEvent() = default;
} // namespace

// NOTE: Use case may be something like
// emit DB_AUTHORIZE(new Request(this, event, &authResponse, 10000))
// database receiver processes if(!request->cancelled())

Request::Request(QObject *parent, const Event& sip, int expires) :
QObject(parent), status(Success), sipEvent(sip), signalled(false)
{
    // compute propogation delay...
    expires -= sip.elapsed() - 20;
    if(expires < 10)
        expires = 10;
    QTimer::singleShot(expires, Qt::CoarseTimer, this, &Request::timeout);
}

Request::Request(QObject *parent, const Event& sip, int expires, Reply method) :
QObject(parent), status(Success), sipEvent(sip), signalled(false)
{
    connect(this, &Request::results, parent, method);

    // compute propogation delay...
    expires -= sip.elapsed() - 20;
    if(expires < 10)
        timeout();
    else
        QTimer::singleShot(expires, Qt::CoarseTimer, this, &Request::timeout);
}

void Request::timeout()
{
    if(!signalled) {
        emit results(Timeout, sipEvent, QList<QSqlRecord>());
        signalled = true;
    }
}

bool Request::cancelled()
{
    if(signalled) {
        deleteLater();
        return true;
    }
    return false;
}

bool Request::event(QEvent *evt)
{
    auto id = static_cast<int>(evt->type());
    if(id < QEvent::User + 1)
        return QObject::event(evt);

    auto reply = dynamic_cast<RequestEvent *>(evt);
    if(!signalled) {
        signalled = true;
        switch(id) {
        case REQUEST_SUCCESS:
            emit results(reply->error(), sipEvent, reply->results());
            break;
        case REQUEST_FAILED:
            emit results(reply->error(), sipEvent, QList<QSqlRecord>());
            break;
        default:
            break;
        }
    }
    deleteLater();
    return true;
}

void Request::notifySuccess(QSqlQuery& results, ErrorResult error)
{
    QList<QSqlRecord> records;
    while(results.next())
        records << results.record();
    QCoreApplication::postEvent(this, 
        new RequestEvent(REQUEST_SUCCESS, error, records));
}

void Request::notifyFailed(ErrorResult error)
{
    QCoreApplication::postEvent(this, 
        new RequestEvent(REQUEST_FAILED, error));
}

