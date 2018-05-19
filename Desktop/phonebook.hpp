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

enum class KeyVerification : int
{
    Unkeyed = 0,    // user has no key
    Initial,        // first trust state, unverified
    Changed,        // key changed, need new verify
    Verified,       // key currently verified
};

class ContactItem final
{
    friend class MemberDelegate;
    friend class MemberModel;
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

    QByteArray key() const {
        return publicKey;
    }

    KeyVerification verify() const {
        return keyVerify;
    }

    QString info() const {
        return extendedInfo;
    }

    QString topic() const {
        return contactTopic;
    }

    QString dialed() const {
        return textNumber;
    }

    QDateTime updated() const {
        return contactUpdated;
    }

    QDateTime sync() const {
        return contactCreated;
    }

    int sequence() const {
        return contactSequence;
    }

    bool isExtension() const {
        return extensionNumber > -1;
    }

    bool isAdmin() const {
        return admin;
    }

    bool isGroup() const {
        return group;
    }

    bool isHidden() const {
        return hidden;
    }

    bool isOnline() const {
        return online;
    }

    bool isSuspended() const {
        return suspended;
    }

    SessionItem *getSession() const {
        return session;
    }

    ContactItem *groupAdmin() const {
        if(!isGroup())
            return nullptr;
        return contactLeader;
    }

    QString filter() const {
        return contactFilter;
    }

    QList<ContactItem*> membership() {
        return contactMembers;
    }

    void add();
    void removeMember(ContactItem *item);

    static QList<ContactItem*> sessions() {
        return groups + users;
    }

    static ContactItem *findContact(int uid) {
        return index[uid];
    }

    static void deauthorize(const QString& id) {
        usrAuths.remove(id);
        grpAuths.remove(id);
    }

    static QStringList allUsers();
    static QStringList allGroups();
    static QList<ContactItem*>findAuth(const QString& auth);
    static ContactItem *findText(const QString& text);
    static ContactItem *findExtension(int number);
    static ContactItem *find(const UString& who);
    static void removeIndex(ContactItem *item);
    static void purge();

private:
    SessionItem *session;
    UString displayName, contactUri, contactTimestamp, contactType;
    UString contactPublic, contactEmail;
    QString contactFilter, contactTopic, textDisplay, textNumber, authUserId;
    QString extendedInfo;
    QDateTime contactCreated, contactUpdated, topicUpdated;
    int contactSequence, topicSequence;
    QByteArray publicKey;
    KeyVerification keyVerify;
    bool group, added, hidden, admin, online, suspended;
    QList<ContactItem *> contactMembers;
    ContactItem *contactLeader;
    int extensionNumber;
    int uid;

    static QList<ContactItem *> users, groups;
    static QHash<UString,ContactItem *> foreign;
    static QHash<int,ContactItem *> index;
    static QSet<QString> usrAuths;
    static QSet<QString> grpAuths;
};

class MemberModel final : public QAbstractListModel
{
    friend class memberDelegate;

public:
    enum {
        DisplayNumber = Qt::UserRole + 1,
        DisplayName,
        DisplayOnline,
        DisplaySuspended,
        DisplaySeparator,
        DisplayMembers,
    };

    MemberModel(ContactItem *item);

    bool isAdmin() const {
        return admin;
    }

    bool isMember(const QModelIndex& index) const {
        if(index.row() >= membership.count())
            return false;
        return true;
    }

    bool isSeparator(const QModelIndex& index) const {
        if(index.row() > 0 && index.row() == membership.count())
            return true;
        return false;
    }

    void updateOnline(ContactItem *item);
    void remove(ContactItem *item);

private:
    QList<ContactItem *> membership;
    QList<ContactItem *> nonmembers;
    bool admin;

    int rowCount(const QModelIndex& parent) const final;
    QVariant data(const QModelIndex& index, int role) const final;
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

    void changeOnline(int number, bool status);
    void remove(int number);

private:
    int rowCount(const QModelIndex& parent) const final;
    QVariant data(const QModelIndex& index, int role) const final;
};

class MemberDelegate final : public QStyledItemDelegate
{
public:
    MemberDelegate(QWidget *parent) : QStyledItemDelegate(parent) {}

private:
    QSize sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const final;
    void paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const final;
    bool eventFilter(QObject *list, QEvent *event) final;
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

    friend class MemberDelegate;
public:
    Phonebook(Desktop *main, Sessions* sessionsPage);

    void enter();
    void setWidth(int width);
    void remove(ContactItem *item);

    static Phonebook *instance() {
        return Instance;
    }

    static ContactItem *self();
    static ContactItem *oper();
    static ContactItem *find(int extension);

private:
    Desktop *desktop;
    LocalDelegate *localPainter;
    LocalContacts *localModel;
    MemberDelegate *memberPainter;
    bool requestPending;
    Connector *connector;
    QTimer refreshRoster;

    static Phonebook *Instance;

    bool event(QEvent *event) final;
    void updateProfile(ContactItem *item);
    void clearGroup();
    void updateGroup();
    void changeMembership(ContactItem *item, const UString& members, const UString& notify, const UString& reason);

signals:
     void changeSessions(Storage *storage, const QList<ContactItem *>& contacts);
     void changeWidth(int width);
     void removeSelf();

private slots:
    void search();
    void filterView(const QString& selected);
    void selectContact(const QModelIndex& index);
    void changeStatus(const QByteArray& bitmap, int first, int last);
    void changeConnector(Connector *connector);
    void changeStorage(Storage *storage);
    void changeProfile();
    void promoteRemote();
    void demoteLocal();
};

#endif

