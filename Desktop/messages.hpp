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

#ifndef MESSAGES_HPP_
#define	MESSAGES_HPP_

#include <QWidget>
#include <QAbstractListModel>
#include <QDateTime>
#include "../Common/types.hpp"

class SessionItem;

class MessageItem final
{
    friend class MessageDelegate;
    friend class MessageModel;

public:
    typedef enum {
        TEXT_MESSAGE,
    } type_t;

    MessageItem(SessionItem *sid, const QString& text); // local outbox text

    type_t type() const {
        return msgType;
    }

    UString subject() const {
        return msgSubject;
    }

    UString from() const {
        return msgFrom;
    }

    UString to() const {
        return msgTo;
    }

    QByteArray body() const {
        return msgBody;
    }

    bool isInbox() const {
        return inbox;
    }

private:
    int dateSequence;               // used to help unique sorting
    unsigned dayNumber;             // used to determine day message is for
    QDateTime dateTime;             // date and time message was created
    SessionItem *session;           // session back pointer
    bool inbox;
    type_t msgType;
    UString msgSubject, msgFrom, msgTo;
    QByteArray msgBody;
};

class MessageModel final : public QAbstractListModel
{
    friend class MessageDelegate;

public:
    MessageModel(QWidget *parent) : QAbstractListModel(parent) {}
        
private:
    int rowCount(const QModelIndex& parent) const final;
    QVariant data(const QModelIndex& index, int role) const final;
};

#endif

