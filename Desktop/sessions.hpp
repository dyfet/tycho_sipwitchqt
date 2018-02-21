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

#ifndef SESSIONS_HPP_
#define	SESSIONS_HPP_

#include <QWidget>
#include <QVariantHash>
#include <QPen>
#include "phonebook.hpp"

class Desktop;
class SessionDelegate;
class MessageItem;

class SessionItem final
{
    friend SessionDelegate;

public:
    SessionItem(ContactItem *contactItem, bool active = false);

    QString display() const {
        if(!contact)
            return "";
        return contact->display();
    }

    QString type() const {
        if(!contact)
            return "NONE";
        return contact->type();
    }

    bool setOnline(bool flag);

private:
    //QList<MessageItem> messages;
    ContactItem *contact;
    QString status;
    QPen color;
    bool online, busy;
};

class SessionModel final : public QAbstractListModel
{
    friend class SessionDelegate;

public:
    SessionModel(QWidget *parent) : QAbstractListModel(parent) {}

    static void purge();

private:
    int rowCount(const QModelIndex& parent) const final;
    QVariant data(const QModelIndex& index, int role) const final;
};

class SessionDelegate final : public QStyledItemDelegate
{
public:
    SessionDelegate(QWidget *parent) : QStyledItemDelegate(parent) {}

private:
    QSize sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const final;
    void paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const final;
};

class Sessions : public QWidget
{
public:
    Sessions(Desktop *main);

    void enter(void);

private:
    Desktop *desktop;
    Connector *connector;
    SessionModel *model;
    SessionDelegate *sessions;

private slots:
    void search();

    void changeActive(const QVariantHash& status);
    void changeStorage(Storage *storage);
    void changeSessions(Storage *storage, const QList<ContactItem*>& contacts);
    void changeConnector(Connector *connector);
};

#endif

