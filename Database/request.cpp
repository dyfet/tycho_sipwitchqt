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

#include "../Common/compiler.hpp"
#include "request.hpp"

#include <QEvent>

enum {
    REQUEST_SUCCESS = QEvent::User + 1,
    REQUEST_FAILED
};


class RequestEvent final : public QEvent
{
    Q_DISABLE_COPY(RequestEvent)
public:
    RequestEvent(int event, Request::ErrorResult error = Request::Success, const QList<QSqlRecord> results = QList<QSqlRecord>()) :
    QEvent(static_cast<QEvent::Type>(event)), Results(results), Error(error) {}

    ~RequestEvent();

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

RequestEvent::~RequestEvent() {}


Request::Request(QObject *parent, const Event& sip, int timeout) :
QObject(parent), sipEvent(sip)
{
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Request::timeout);
    timer->setSingleShot(true);
    timer->setInterval(timeout);
    timer->start();
}

void Request::timeout()
{
    timer->stop();
    emit results(Timeout, sipEvent, QList<QSqlRecord>());
}

bool Request::event(QEvent *evt)
{
    int id = static_cast<int>(evt->type());
    if(id < QEvent::User + 1)
        return QObject::event(evt);

    auto reply = static_cast<RequestEvent *>(evt);
    if(timer->isActive()) {
        timer->stop();
        switch(id) {
        case REQUEST_SUCCESS:
            emit results(reply->error(), sipEvent, reply->results());
            break;
        case REQUEST_FAILED:
            emit results(reply->error(), sipEvent, QList<QSqlRecord>());
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

