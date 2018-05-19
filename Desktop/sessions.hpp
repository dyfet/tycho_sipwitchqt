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
#include <QMap>
#include "phonebook.hpp"
#include "messages.hpp"

class Desktop;
class SessionDelegate;
class SessionModel;
class Listener;

class SessionItem final
{
    friend class SessionDelegate;
    friend class SessionModel;
    friend class MessageDelegate;
    friend class MessageModel;
    friend class MessageItem;
    friend class Sessions;

    Q_DISABLE_COPY(SessionItem)
public:
    SessionItem(ContactItem *contactItem, bool active = false);
    ~SessionItem();

    int number() const {
        if(!contact)
            return -1;
        return contact->number();
    }

    QString display() const {
        if(!contact)
            return callDisplay;
        return contact->display();
    }

    QString type() const {
        if(!contact)
            return "CALL";
        return contact->type();
    }

    QString text() const {
        return inputText;
    }

    UString uri() const {
        if(!contact)
            return UString();
        return contact->uri();
    }

    MessageModel *model() const {
        return messageModel;
    }

    int count() const {
        return filtered.count();
    }

    ContactItem *contactItem() const {
        return contact;
    }

    int contactUid() const {
        if(contact)
            return contact->uid;
        return 0;
    }

    UString topic() const {
        if(contact)
            return contact->topic();
        return "None";
    }

    QModelIndex lastMessage() const {
        return messageModel->last();
    }

    bool isLoaded() const {
        return loaded;
    }

    bool isGroup() const {
        if(!contact)
            return false;
        return contact->isGroup();
    }

    bool isHidden() const {
        if(!contact)
            return false;
        return contact->isHidden();
    }

    bool isOnline() const {
        if(!contact)
            return true;
        return contact->isOnline();
    }

    bool isSuspended() const {
        if(!contact)
            return false;
        return contact->isSuspended();
    }

    void setText(const QString& text) {
        inputText = text;
    }

    void clearSearch() {
        foreach(auto msg, filtered) {
            msg->clearSearch();
        }
    }

    QStringList topicList() {
        topics.remove("");
        auto list = topics.toList();
        list.sort();
        return list;
    }

    unsigned unread() {
        return unreadCount;
    }

    void setTopic(const QString& topic, const QDateTime& changed = QDateTime::currentDateTime(), int sequence = 0);
    void addMessage(MessageItem *item);
    void clearUnread();
    void clearMessages();
    void addUnread();
    unsigned loadMessages();
    void close();
    QString title();

private:
    QList<MessageItem *> messages;
    QList<MessageItem *> filtered;
    MessageModel *messageModel;
    QDateTime mostRecent;                       // most recent message
    int lastSequence;                           // highest sequence
    QString inputText;                          // current input buffer
    QString callDisplay;                        // transitory call name
    QSet<QString> topics;
    int cid, did;                               // exosip call info
    bool saved;                                 // whether uses database...
    unsigned unreadCount;                       // unread message count
    ContactItem *contact;
    QString status;
    QPen color;
    bool online, busy, loaded;

    static unsigned totalUnread;
};

class SessionModel final : public QAbstractListModel
{
    friend class SessionDelegate;

public:
    SessionModel(QWidget *parent) : QAbstractListModel(parent) {
        Instance = this;
    }

    ~SessionModel() final {
        Instance = nullptr;
    }

    void changeLayout() {
        layoutAboutToBeChanged();
        layoutChanged();
    }

    void add(SessionItem *item);
    void remove(SessionItem *item);
    void latest(SessionItem *item);
    void clickSession(int row);

    static SessionModel *instance() {
        return Instance;
    }

    static void purge();
    static void clear();
    static void update(SessionItem *session);

private:
    int rowCount(const QModelIndex& parent) const final;
    QVariant data(const QModelIndex& index, int role) const final;

    static SessionModel *Instance;
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
    Q_OBJECT
public:
    Sessions(Desktop *main);

    void enter();
    void listen(Listener *listener);
    QList<MessageItem *> searchMessages(const QString &searchTerm);
    void clickedText(const QString& text, enum ClickedItem);
    void setWidth(int width);
    void remove(ContactItem *item);

    static Sessions *instance() {
        return Instance;
    }

    static SessionItem *active();
    static SessionItem *current();
    static QModelIndex top();

private:
    Desktop *desktop;
    Connector *connector;
    SessionModel *model;
    QString selfName;
    int elideWidth;

    static Sessions *Instance;

    void resizeEvent(QResizeEvent *event) final;
    bool event(QEvent *event) final;

    void finishInput(const QString &error, const QDateTime &timestamp = QDateTime(), int sequence = 0);
    void scrollToBottom();

signals:
    void changeWidth(int width);

public slots:
    void changeSessions(Storage *storage, const QList<ContactItem*>& contacts);
    void activateSession(SessionItem *item);
    void activateContact(ContactItem *item);
    void changeTopic(const QString& topic);
    void closeSession();
    void clearSessions();
    void activateSelf();
    void refreshFont();

private slots:
    void search();
    void setSelf(const QString& text);
    void selectSelf();
    void expireMessages();
    void selectSession(const QModelIndex& index);
    void changeStorage(Storage *storage);
    void changeConnector(Connector *connector);
    void createMessage();
    void checkInput(const QString& text);
    void receiveText(const UString& sipFrom, const UString& sipTo, const UString& text, const QDateTime &timestamp, int sequence, const UString &subject, const UString &type);
};

#endif

