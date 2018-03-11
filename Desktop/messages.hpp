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
#include <QTextLayout>
#include <QTextLine>
#include <QStaticText>
#include "../Common/types.hpp"
#include "phonebook.hpp"

class SessionItem;

class MessageItem final
{
    friend class MessageDelegate;
    friend class MessageModel;

    Q_DISABLE_COPY(MessageItem)
public:
    typedef enum {
        TEXT_MESSAGE,
    } type_t;

    MessageItem(SessionItem *sid, const QString& text, const QDateTime& timestamp, int sequence, bool save = true);
    MessageItem(const QSqlRecord& record);      // database load...

    type_t type() const {
        return msgType;
    }

    UString subject() const {
        return msgSubject;
    }

    ContactItem *from() const {
        return msgFrom;
    }

    ContactItem *to() const {
        return msgTo;
    }

    QByteArray body() const {
        return msgBody;
    }

    bool isInbox() const {
        return inbox;
    }

    bool isValid() const {
        return session != nullptr;
    }

    QDateTime posted() const {
        return dateTime;
    }

    int sequence() const {
        return dateSequence;
    }

    QSize layout(const QStyleOptionViewItem& style, int row);

private:
    int dateSequence;                   // used to help unique sorting
    unsigned dayNumber;                 // used to determine day message is for
    QDateTime dateTime;                 // date and time message was created
    SessionItem *session;               // session back pointer
    bool inbox;
    type_t msgType;
    UString msgSubject;
    ContactItem *msgTo, *msgFrom;
    QByteArray msgBody;

    int lastLayout;                     // whether size hinting needs recomputing
    QSize lastHint;                     // size hint of last layout
    QSize textHint;                     // area of text...
    bool dateHint, userHint, timeHint;  // hinting for time header & user change
    double dateHeight, userHeight, textHeight, leadHeight;

    QStaticText textStatus, textDisplay, textTimestamp, textDateline;
    QString textString, userString;
    QColor itemColor;
    QTextLayout textLayout;
    QList<QTextLayout::FormatRange> textFormats, userFormats;
    QList<QTextLine> textLines;
};

class MessageModel final : public QAbstractListModel
{
    friend class MessageDelegate;

public:
    MessageModel(SessionItem *list) : QAbstractListModel(), session(list) {}
        
    void fastUpdate(int count = 1);
    bool add(MessageItem *item);
    QModelIndex last();

    void changeLayout();

private:
    SessionItem *session;

    int rowCount(const QModelIndex& parent) const final;
    QVariant data(const QModelIndex& index, int role) const final;
};

class MessageDelegate final : public QStyledItemDelegate
{
public:
    MessageDelegate(QWidget *parent);

private:
    QSize sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const final;
    void paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const final;
};

#endif

