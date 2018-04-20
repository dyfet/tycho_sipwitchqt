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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class Desktop;
class Storage;
class SessionItem;
class Sessions;

class ContactItem final
{
    friend class LocalDelegate;
    friend class LocalContacts;
    friend class MessageDelegate;
    friend class SessionItem;
    friend class MessageItem;

public:
    ContactItem(const QSqlRecord& record);

    int number() const {
        return extensionNumber;
    }

    int id() const {
        return uid;
    }

    UString display() const {
        return displayName;
    }

    UString type() const {
        return contactType;
    }

    UString user() const {
        return authUserId;
    }

    UString uri() const {
        return contactUri;
    }

    UString publicUri() const {
        return contactPublic;
    }

    UString mailTo() const {
        return contactEmail;
    }

    UString timestamp() const {
        return contactTimestamp;
    }

    QString info() const {
        return extendedInfo;
    }

    QString dialed() const {
        return textNumber;
    }

    QDateTime updated() const {
        return contactUpdated;
    }

    bool isExtension() const {
        return extensionNumber > -1;
    }

    bool isGroup() const {
        return group;
    }

    SessionItem *getSession() const {
        return session;
    }

    QString filter() const {
        return contactFilter;
    }

    void add();

    static QList<ContactItem*> sessions() {
        return groups + users;
    }

    static ContactItem *findContact(int uid) {
        return index[uid];
    }

    static QStringList allUsers();
    static QStringList allGroups();
    static ContactItem *findText(const QString& text);
    static ContactItem *findExtension(int number);
    static ContactItem *find(const UString& who);
    static void purge();

private:
    SessionItem *session;
    UString displayName, contactUri, contactTimestamp, contactType;
    UString contactPublic, contactEmail;
    QString contactFilter, textDisplay, textNumber, authUserId;
    QString extendedInfo;
    QDateTime contactUpdated;
    bool group, added;
    int extensionNumber;
    int uid;

    static QList<ContactItem *> users, groups;
    static QHash<UString,ContactItem *> foreign;
    static QHash<int,ContactItem *> index;
    static QSet<QString> usrAuths;
    static QSet<QString> grpAuths;
};

class LocalContacts final : public QAbstractListModel
{
    friend class LocalDelegate;

public:
    LocalContacts(QWidget *parent) : QAbstractListModel(parent) {}
    void setFilter(const UString& filter);
    ContactItem *updateContact(const QJsonObject& json);
    void clickContact(int row);

    void changeLayout() {
        layoutAboutToBeChanged();
        layoutChanged();
    }

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
    Q_OBJECT
public:
    Phonebook(Desktop *main, Sessions* sessionsPage);

    void enter();
    void setWidth(int width);

    static Phonebook *instance() {
        return Instance;
    }

    static ContactItem *self();

private:
    Desktop *desktop;
    LocalDelegate *localPainter;
    LocalContacts *localModel;
    bool requestPending;
    Connector *connector;
    QTimer refreshRoster;

    static Phonebook *Instance;

    bool event(QEvent *event) final;
    void updateProfile(ContactItem *item);

signals:
     void changeSessions(Storage *storage, const QList<ContactItem *>& contacts);
     void changeWidth(int width);

private slots:
    void search();
    void filterView(const QString& selected);
    void selectContact(const QModelIndex& index);
    void changeConnector(Connector *connector);
    void changeStorage(Storage *storage);
    void changeProfile();
    void promoteRemote();
    void demoteLocal();
};

#endif

