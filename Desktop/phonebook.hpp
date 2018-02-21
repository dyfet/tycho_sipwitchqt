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

#ifndef PHONEBOOK_HPP_
#define	PHONEBOOK_HPP_

#include "../Common/types.hpp"
#include "../Connect/connector.hpp"

#include <QWidget>
#include <QVariantHash>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QSqlRecord>
#include <QDateTime>
#include <QModelIndex>
#include <QVector>

class Desktop;
class Storage;
class LocalContacts;
class LocalDelegate;
class SessionItem;

class ContactItem final
{
    friend LocalDelegate;
    friend SessionItem;

public:
    ContactItem(const QSqlRecord& record);

    int number() const {
        return extensionNumber;
    }

    UString display() const {
        return displayName;
    }

    UString type() const {
        return contactType;
    }

    UString uri() const {
        return contactUri;
    }

    UString timestamp() const {
        return contactTimestamp;
    }

    QDateTime updated() const {
        return contactUpdated;
    }

    bool isExtension() const {
        return extensionNumber > -1;
    }

    QString filter() const {
        return contactFilter;
    }

    static QList<ContactItem*> sessions() {
        return groups + users;
    }

    static void purge();

private:
    ContactItem *prior;
    UString displayName, contactUri, contactTimestamp, contactType;
    QString contactFilter, textDisplay, textNumber;
    QDateTime contactUpdated;
    int extensionNumber;
    int uid;

    static ContactItem *list;
    static QList<ContactItem *> users, groups;
};

class LocalContacts final : public QAbstractListModel
{
    friend class LocalDelegate;

public:
    LocalContacts(QWidget *parent) : QAbstractListModel(parent) {}
    void setFilter(const UString& filter);

private:    
    int rowCount(const QModelIndex& parent) const final;
    QVariant data(const QModelIndex& index, int role) const final;
};

class LocalDelegate final : public QStyledItemDelegate
{
public:
    LocalDelegate(QWidget *parent) : QStyledItemDelegate(parent) {}

private:
    QSize sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const final;
    void paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const final;
};

class Phonebook final : public QWidget
{
public:
    Phonebook(Desktop *main);
    ~Phonebook() = default;

    void enter();
    ContactItem *self();

private:
    Desktop *desktop;
    LocalDelegate *localPainter;
    LocalContacts *localModel;
    Connector *connector;

private slots:
    void search();
    void filter(const QString& selected);
    void select(const QModelIndex& index);
    
    void changeConnector(Connector *connector);
    void changeStorage(Storage *storage);
};

#endif

