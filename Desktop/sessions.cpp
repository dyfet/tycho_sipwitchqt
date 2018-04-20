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

#include "desktop.hpp"
#include "ui_sessions.h"

#include <QPainter>
#include <QScrollBar>
#include <QDesktopServices>

#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif

static Ui::SessionsWindow ui;
static int activeCalls = 0;                 // active calls (temp sessions)...
static QList<SessionItem*> sessions;
static QMap<QString, SessionItem *> groups;
static MessageItem *activeMessage = nullptr;
static SessionItem *clickedItem = nullptr;
static SessionItem *inputItem = nullptr;
static SessionItem *activeItem = nullptr, *local[1000];
static QString userStatus = "@", groupStatus = "#";
static QPen groupActive("blue"), groupDefault("black"), unreadStatus("red");
static QPen onlineUser("green"), offlineUser("black"), busyUser("yellow");
static bool mousePressed = false;
static QPoint mousePosition;
static int cellHeight, cellLift = 3;

Sessions *Sessions::Instance = nullptr;
SessionModel *SessionModel::Instance = nullptr;
unsigned SessionItem::totalUnread = 0;

SessionItem::SessionItem(ContactItem *contactItem, bool active) :
messageModel(nullptr)
{
    contact = contactItem;
    contact->session = this;

    messageModel = new MessageModel(this);

    if(contactItem->isGroup()) {
        status = groupStatus;
        if(active)
            color = groupActive;
        else
            color = groupDefault;
    }
    else {
        status = userStatus;
        if(active)
            color = onlineUser;
        else
            color = offlineUser;
    }

    online = active;
    busy = loaded = false;
    cid = did = -1;
    saved = true;
    unreadCount = 0;

    auto number = contactItem->number();
    if(number > -1 && number < 1000)
        local[number] = this;
}

SessionItem::~SessionItem()
{
    if(contact) {
        contact->session = nullptr;
        contact = nullptr;
    }

    // clear out messages...

    if(messageModel) {
        delete messageModel;
        messageModel = nullptr;
    }

    foreach(auto msg, messages) {
        delete msg;
    }
}

void SessionItem::clearUnread()
{
    if(!unreadCount)
        return;

    totalUnread -= unreadCount;
    unreadCount = 0;
    if(isGroup())
        color = groupDefault;

    SessionModel::update(this);
    Desktop::setUnread(totalUnread);
}

void SessionItem::addUnread()
{
    ++unreadCount;
    ++totalUnread;

    if(isGroup())
        color = groupActive;

    SessionModel::update(this);
    Desktop::setUnread(totalUnread);
}

void SessionItem::addMessage(MessageItem *msg)
{
    Q_ASSERT(msg != nullptr);
    Q_ASSERT(messageModel != nullptr);

    if(messageModel->add(msg))
        messageModel->fastUpdate();

    // if fill-in, we can just post and finish...
    if(contact->contactUpdated > msg->posted())
        return;

    auto storage = Storage::instance();
    Q_ASSERT(storage != nullptr);
    storage->runQuery("UPDATE Contacts SET last=?, sequence=? WHERE uid=?;", {
                          msg->posted(), msg->sequence(), contact->uid});
}

unsigned SessionItem::loadMessages()
{
    unsigned count = 0;
    auto fast = 0;

    QDateTime latest = contact->contactUpdated;
    int sequence = 0;

    if(loaded || contact == nullptr)
        return count;

    auto storage = Storage::instance();
    Q_ASSERT(storage != nullptr);
    Q_ASSERT(messageModel != nullptr);

    auto query = storage->getRecords("SELECT * FROM Messages WHERE sid=? ORDER BY posted DESC, seqid DESC;", {contact->uid});

    while(query.isActive() && query.next()) {
        auto msg = new MessageItem(query.record());
        if(!msg->isValid()) {
            qDebug() << "FAILED MESSAGE";
            delete msg;
            continue;
        }
        if(msg->posted() > latest) {
            latest = msg->posted();
            sequence = msg->sequence();
        }
        if(messageModel->add(msg))
            ++fast;
        ++count;
    }

    messageModel->fastUpdate(fast);
    loaded = true;
    qDebug() << "Loaded" << fast << "/" << count << "message(s) for" << contact->displayName;
    if(latest > contact->contactUpdated)
        storage->runQuery("UPDATE Contacts SET last=?, sequence=? WHERE uid=?;", {latest, sequence, contact->uid});

    return count;
}

QList <MessageItem *> Sessions::searchMessages(const QString& searchTerm){
    QList <MessageItem *> result = {};
    foreach (auto singleMessage, activeItem->messages) {
        singleMessage->clearSearch();
        int len = searchTerm.length();
        if (QString::fromUtf8(singleMessage->body()).contains(searchTerm)) {
            int pos = 0;
            while((pos =  singleMessage->body().indexOf(searchTerm, pos)) > -1 ) {
                singleMessage->addSearch(pos, len);
                ++pos;
            }
            result += singleMessage;
        }
    }
   return result;
}

QString SessionItem::title()
{
    return status + contact->textNumber + " " + contact->textDisplay;
}

bool SessionItem::setOnline(bool flag)
{
    std::swap(flag, online);
    if(!online)
        busy = false;
    return flag;
}

void SessionModel::add(SessionItem *item)
{
    int row;
    if(item->isGroup()) {
        row = static_cast<int>(std::distance(groups.begin(), groups.lowerBound(item->display())));
        groups[item->display()] = item;
    }
    else {
        row = groups.count() + activeCalls;
    }

    beginInsertRows(QModelIndex(), row, row);
    sessions.insert(row, item);
    endInsertRows();
}

void SessionModel::clickSession(int row)
{
    if(row < 0 || row >= sessions.count())
        return;

    clickedItem = sessions[row];
    dataChanged(index(row), index(row));
    QTimer::singleShot(CONST_CLICKTIME, this, [=] {
        if(row < sessions.count())
            dataChanged(index(row), index(row));
    });
}


int SessionModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    if(sessions.size() < 1)
        return 0;
    return sessions.size();
}

QVariant SessionModel::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    int rows = rowCount(index);
    if(row < 0 || row >= rows || role!= Qt::DisplayRole)
        return QVariant();

    auto item = sessions[row];
    if(item->display().isEmpty())
        return QVariant();

    return item->display();
}

void SessionModel::purge()
{
    ui.messages->setModel(nullptr);
    activeItem = nullptr;
    SessionItem::totalUnread = 0;

    if(activeMessage) {
        delete activeMessage;
        activeMessage = nullptr;
    }

    memset(local, 0, sizeof(local));
    foreach(auto session, sessions) {
        session->contact = nullptr;     // avoid dependency during takedown...
        delete session;
    }
    sessions.clear();
    groups.clear();
    Desktop::setUnread(0);
}

void SessionModel::update(SessionItem *session)
{
    if(!Instance)
        return;

    auto count = sessions.indexOf(session);

    if(count < activeCalls)
        return;

    if(count >= groups.count() + activeCalls) {
refresh:
        Instance->dataChanged(Instance->index(count), Instance->index(count));
        return;
    }

    QString old = "";
    foreach(auto key, groups.keys()) {
        if(groups[key] == sessions[count]) {
            old = key;
            break;
        }
    }

    if(old.isEmpty())
        return;

    auto item = session->contact;
    if(old == item->display())
        goto refresh;

    auto old_row = std::distance(groups.begin(), groups.find(old));
    auto new_row = std::distance(groups.begin(), groups.upperBound(item->display()));
    if(old_row < new_row)
        --new_row;
    if(old_row == new_row) {
        groups.take(old);
        groups[item->display()] = session;
        goto refresh;
    }
    sessions.takeAt(static_cast<int>(old_row));
    sessions.insert(static_cast<int>(new_row), session);
    if(old_row < new_row)
        Instance->dataChanged(Instance->index(static_cast<int>(old_row)), Instance->index(static_cast<int>(new_row)));
    else
        Instance->dataChanged(Instance->index(static_cast<int>(new_row)), Instance->index(static_cast<int>(old_row)));

}

QSize SessionDelegate::sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    Q_UNUSED(style);

    auto row = index.row();
    auto rows = sessions.size();

    if(row < 0 || row >= rows)
        return {0, 0};

    auto spacing = 0;
    auto item = sessions[row];
    auto contact = item->contact;
    if(contact == nullptr || item->display().isEmpty())
        return {0, 0};

    if(row == groups.size())
        spacing += 4;

    return {ui.sessions->width(), cellHeight + spacing};
}

void SessionDelegate::paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    if(style.rect.isEmpty())
        return;

    auto pen = painter->pen();
    auto row = index.row();
    auto item = sessions[row];
    auto pos = style.rect.bottomLeft();
    QColor color("black");

    // separate active calls and groups from users...
    if(row == groups.size() + activeCalls)
        pos.ry() += 4;
    else if(activeCalls > 0 && row == activeCalls)
        pos.ry() += 4;

    // line up with witch icon, draw status...
    pos.rx() += 4;

    // if clicked...
    if(item == clickedItem) {
        auto rect = QRect(pos.x(), pos.y() - cellHeight, style.rect.width() - 4, cellHeight);
        painter->fillRect(rect, QColor(CONST_CLICKCOLOR));
        clickedItem = nullptr;
    }

    pos.ry() -= cellLift;  // off bottom
    painter->setPen(item->color);
    painter->drawText(pos, item->status);

    // show display name...maybe later add elipsis...
    auto metrics = painter->fontMetrics();
    auto width = style.rect.width() - 16;
    if(item->unreadCount > 0)
        width -= 24;
    auto text = metrics.elidedText(item->display(), Qt::ElideRight, width);

    pos.rx() += 16;
    painter->setPen(pen);
    painter->drawText(pos, text);

    QString unreadText = "";
    if(item->unreadCount > 99)
        unreadText="**";
    else if(item->unreadCount)
        unreadText= QString::number(item->unreadCount);
    else
        return;

    pos.setX(style.rect.x() + style.rect.width() - 20);
    painter->setPen(unreadStatus);
    painter->drawText(pos, unreadText);
    painter->setPen(pen);

}

Sessions::Sessions(Desktop *control) :
QWidget(), desktop(control), model(nullptr)
{
    ui.setupUi(static_cast<QWidget *>(this));
    ui.sessions->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui.messages->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui.expand->setVisible(false);

    Q_ASSERT(Instance == nullptr);
    Instance = this;
    cellHeight = QFontInfo(desktop->getBasicFont()).pixelSize() + 5;

    connect(Toolbar::search(), &QLineEdit::returnPressed, this, &Sessions::search);
    connect(desktop, &Desktop::changeStorage, this, &Sessions::changeStorage);
    connect(desktop, &Desktop::changeConnector, this, &Sessions::changeConnector);
    connect(ui.sessions, &QListView::clicked, this, &Sessions::selectSession);
    connect(ui.self, &QPushButton::pressed, this, &Sessions::selectSelf);
    connect(ui.status, &QPushButton::pressed, this, &Sessions::selectSelf);
    connect(ui.input, &QLineEdit::returnPressed, this, &Sessions::createMessage);
    connect(ui.input, &QLineEdit::textChanged, this, &Sessions::checkInput);
    connect(desktop ,&Desktop::changeFont,this,&Sessions::refreshFont);

    connect(ui.bottom, &QPushButton::pressed, this, [this]() {
        scrollToBottom();
    });

    connect(control, &Desktop::changeSelf, this, [](const QString& text) {
        ui.self->setText(text);
    });

    SessionModel::purge();
    ui.sessions->setItemDelegate(new SessionDelegate(this));
    ui.messages->setItemDelegate(new MessageDelegate(ui.messages));
}

bool Sessions::event(QEvent *event)
{
    switch(event->type()) {
    case QEvent::MouseButtonPress: {
        auto mpos = static_cast<QMouseEvent *>(event)->pos();
        auto rect = QRect(ui.separator->pos(), ui.separator->size());
        rect.setLeft(rect.left() - 2);
        rect.setRight(rect.right() + 2);
        if(rect.contains(mpos)) {
            qDebug() << "MOUSE" << mpos << "VS" << rect;
            mousePosition = mpos;
            mousePressed = true;
            ui.separator->setStyleSheet("color: red;");
        }
        break;
    }
    case QEvent::MouseMove: {
        if(mousePressed) {
            auto mpos = static_cast<QMouseEvent *>(event)->pos();
            auto width = ui.sessions->maximumWidth() + mpos.x() - mousePosition.x();
            auto maxWidth = size().width() / 3;
            if(width < 132 || width > maxWidth) {
                mousePressed = false;
                ui.separator->setStyleSheet(QString());
                break;
            }
            mousePosition.setX(mpos.x());
            ui.sessions->setMaximumWidth(width);
            emit changeWidth(width);
            if(model)
                model->changeLayout();
            if(activeItem && activeItem->model())
                activeItem->model()->changeLayout();
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        if(mousePressed) {
            ui.separator->setStyleSheet(QString());
            mousePressed = false;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::event(event);
}

void Sessions::setWidth(int width)
{
    if(width < 132)
        width = 132;
    if(width > size().width() / 3)
        width = size().width() / 3;

    ui.sessions->setMaximumWidth(width);
}

void Sessions::resizeEvent(QResizeEvent *event)
{
    static bool active = false;

    QWidget::resizeEvent(event);
    if(!active && activeItem) {
        active = true;
        QTimer::singleShot(60, this, [](){
            active = false;
            if(activeItem && activeItem->model())
                activeItem->model()->changeLayout();
        });
    }
}

SessionItem *Sessions::active()
{
    return activeItem;
}

void Sessions::enter()
{
    Toolbar::search()->setText("");
    Toolbar::search()->setPlaceholderText(tr("Search messages here"));
    Toolbar::search()->setEnabled(true);
    mousePressed = false;
    ui.separator->setStyleSheet(QString());

    if(activeItem) {
        Toolbar::setTitle(activeItem->title());
        ui.input->setFocus();
    }
    else {
        activateSelf();
        Toolbar::search()->setFocus();
    }
}

void Sessions::listen(Listener *listener)
{
    connect(listener, &Listener::receiveText, this, &Sessions::receiveText);
}

void Sessions::clickedText(const QString& text, enum ClickedItem type)
{
    qDebug() << "TYPE" << type;
    switch(type) {
    case EXTENSION_CLICKED: {
        ContactItem *item = nullptr;
        auto number = text.mid(1).toInt();
        if(number >= 0 && number < 1000)
            item = ContactItem::findExtension(number);
        if(item) {
            activateContact(item);
            desktop->statusMessage("");
            return;
        }
        desktop->errorMessage("Extension " + text.mid(1) + " not found");
        return;
    }
    case MAP_CLICKED: {
        qDebug() << "GEO COORDINATE FUTURE MAP" << text;
        return;
    }
    case NUMBER_CLICKED: {
        auto nbr = text;
        nbr.replace(" ", "");
        qDebug() << "phone nbr" << nbr;
        return;
    }
    case URL_CLICKED: {
        if(text.left(7) != "http://" && text.left(8) != "https://")
            QDesktopServices::openUrl("http://" + text);
        else
            QDesktopServices::openUrl(text);
        return;
    }
    default:
        return;
    }
}

void Sessions::search()
{
    if(!desktop->isCurrent(this))
        return;

    UString text = Toolbar::search()->text().toUtf8();
    Toolbar::search()->setText("");
    if(text.isEmpty()) {
        if(activeItem) {
            activeItem->filtered = activeItem->messages;
            activeItem->clearSearch();
            activeItem->model()->changeLayout();
        }
        return;
    }

    if(!activeItem){
        ContactItem *item = nullptr;

        if(text.isNumber()) {
            int number = text.toInt();
            if(number >= 0 && number < 1000)
                item = ContactItem::findExtension(number);
        } else
            item = ContactItem::findText(QString::fromUtf8(text));

        if(item) {
            activateContact(item);
            desktop->statusMessage("");
            return;
        } else{
        QChar quote = '\"';
        desktop->errorMessage(quote + QString::fromUtf8(text) + quote + tr(" not found or not unique"));
        }
    }
    else{
        activeItem->filtered = searchMessages(QString::fromUtf8(text));
        qDebug() << searchMessages(QString::fromUtf8(text)) << endl;
        activeItem->model()->changeLayout();
    }

//just testing whether jobs work now.

}

void Sessions::activateContact(ContactItem* contact)
{
    if(!desktop->isCurrent(this))
        desktop->showSessions();

    if(!contact || contact == Phonebook::self()) {
        activateSession(nullptr);
        return;
    }

    auto session = contact->getSession();

    if(session) {
        activateSession(session);
        return;
    }
    session = new SessionItem(contact);
    model->add(session);
    activateSession(session);
}

void Sessions::activateSession(SessionItem* item)
{
    if(!item) {
        activateSelf();
        return;
    }
    if(activeItem)
        activeItem->setText(ui.input->text());

    activeItem = item;
    ui.messages->setModel(item->model());
    ui.inputFrame->setVisible(true);
    ui.input->setText(item->text());
    ui.input->setFocus();
    activeItem->filtered = activeItem->messages;
    activeItem->clearSearch();
    Toolbar::setTitle(item->title());
    Toolbar::search()->setPlaceholderText("Search messages");
    item->loadMessages();
    item->clearUnread();
    scrollToBottom();
}

void Sessions::refreshFont()
{
    auto old = ui.messages->itemDelegate();
    ui.messages->setItemDelegate(new MessageDelegate(ui.messages));
    delete old;
    activeItem->model()->changeLayout();

}

void Sessions::selectSession(const QModelIndex& index)
{
    auto row = index.row();
    if(row < 0 || row > 999)
        return;

    auto item = sessions[row];
    if(!item)
        return;

    model->clickSession(row);
    activateSession(item);
}

void Sessions::activateSelf()
{
    if(activeItem)
        activeItem->setText(ui.input->text());

    activeItem = nullptr;
    Toolbar::setTitle("");
    Toolbar::search()->setFocus();
    ui.input->setText("");
    ui.inputFrame->setVisible(false);
    ui.toolFrame->setVisible(false);
    ui.messages->setModel(nullptr);
    Toolbar::search()->setPlaceholderText(tr("Select extension or session"));

}

void Sessions::selectSelf()
{
    ui.self->setStyleSheet("border: none; outline: none; margin: 0px; padding: 0px; text-align: left; background: " CONST_CLICKCOLOR ";");
    if(connector)
        ui.status->setStyleSheet("color: green; border: none; outline: none; margin: 0px; padding: 0px; text-align: left; background: " CONST_CLICKCOLOR ";");
    else
        ui.status->setStyleSheet("color: black; border: none; outline: none; margin: 0px; padding: 0px; text-align:left; background: " CONST_CLICKCOLOR ";");

    activateSelf();
    QTimer::singleShot(CONST_CLICKTIME, this, [this] {
        ui.self->setStyleSheet("border: none; outline: none; margin: 0px; padding: 0px; text-align: left; background: white;");
        if(connector)
            ui.status->setStyleSheet("color: green; border: none; outline: none; margin: 0px; padding: 0px; text-align: left; background: white;");
        else
            ui.status->setStyleSheet("color: black; border: none; outline: none; margin: 0px; padding: 0px; text-align:left; background: white;");
    });
}

void Sessions::changeStorage(Storage *storage)
{
    if(storage == nullptr) {
        activateSession(nullptr);
        SessionModel::purge();
        if(model) {
            delete model;
            model = nullptr;
        }
        ui.sessions->setModel(nullptr);
    }
    else {
        auto self = storage->credentials().value("display").toString();
        if(!self.isEmpty())
            ui.self->setText(self);
    }
}

void Sessions::changeSessions(Storage* storage, const QList<ContactItem *>& contacts)
{
    Q_ASSERT(storage != nullptr);

    qDebug() << "*** SESSIONS FOR" << contacts.count();

    if(model) {
        delete model;
        model = nullptr;
    }

    // groups anchored, name sorted...
    SessionModel::purge();
    foreach(auto contact, contacts) {
        auto item = new SessionItem(contact);
        if(item->isGroup())
            groups[item->display()] = item;
        else
            sessions << item;
    }

    int index = 0;
    foreach(auto item, groups.values())
        sessions.insert(index++, item);

    model = new SessionModel(this);
    ui.sessions->setModel(model);
    qDebug() << groups.size() << "GROUPS LOADED";
}

void Sessions::changeConnector(Connector *connected)
{
    connector = connected;
    if(connector) {
        // if we connect/reconnect, input editing is enabled.
        ui.input->setEnabled(true);
        if(ui.inputFrame->isVisible())
            ui.input->setFocus();
        ui.status->setStyleSheet("color: green; border: 0px; margin: 0px; padding: 0px; text-align: left; background: white;");

        connect(connector, &Connector::messageResult, this, [this](int error, const QDateTime &timestamp, int sequence) {
            if(!inputItem)
                return;

            if(error == SIP_OK)
                finishInput("", timestamp, sequence);
            else
                finishInput(tr("Failed to send"));
        }, Qt::QueuedConnection);

        connect(connector, &Connector::syncPending, this, [this](const QByteArray& json) {
            auto jdoc = QJsonDocument::fromJson(json);
            auto list = jdoc.array();
            foreach(auto obj, list) {
                auto msg = obj.toObject();
                UString from = msg["f"].toString().toUtf8();
                UString to = msg["t"].toString().toUtf8();
                UString subject = msg["s"].toString().toUtf8();
                auto timestamp = QDateTime::fromString(msg["p"].toString(), Qt::ISODate);
                auto sequence = msg["u"].toInt();
                if(msg["c"] == "text") {
                    UString text = msg["b"].toString().toUtf8();
                    receiveText(from, to, text, timestamp, sequence, subject);
                }
            }
            auto storage = Storage::instance();
            foreach(auto session, sessions) {
                auto uid = session->contact->id();
                if(storage)
                    storage->runQuery("UPDATE Contacts SET last=?, sequence=? WHERE uid=?;",
                        {session->mostRecent, session->lastSequence, uid});
            }
            connector->ackPending();    // let server know we processed...
        }, Qt::QueuedConnection);

    }
    else {
        // if we loose connection, we clear any pending send, and disable further input.
        activeMessage = nullptr;
        ui.input->setEnabled(false);
        ui.status->setStyleSheet("color: black; border: 0px; margin: 0px; padding: 0px; text-align:left; background: white;");
    }
}

void Sessions::createMessage()
{
    if(ui.input->text().isEmpty())
        return;

    // get what we need to send message...
    auto target = activeItem->contact->uri();

    if(!connector) {
        desktop->errorMessage(tr("Not online"));
        return;
    }

    if(!connector->sendText(target, ui.input->text().toUtf8())) {
        desktop->errorMessage(tr("Send failed"));
        return;
    }

    // pending send confirmation state...
    ui.input->setEnabled(false);
    inputItem = activeItem;
    inputItem->setText(ui.input->text());
    ui.messages->setFocus();
}

void Sessions::receiveText(const UString& sipFrom, const UString& sipTo, const UString& text, const QDateTime& timestamp, int sequence, const UString& subject)
{
    qDebug() << "*** MESSAGE FROM" << sipFrom << "TO" << sipTo << "TEXT" << text << "SYNC" << timestamp << sequence;

    auto from = ContactItem::find(sipFrom);
    auto to = ContactItem::find(sipTo);
    auto self = Phonebook::self();
    ContactItem *sid = nullptr;

    if(!from || !to) {
        qDebug() << "Message cannot be connected; lost";
        return;
    }

    // inbox vs outbox
    if(to == self || from->isGroup())
        sid = from;
    else
        sid = to;

    // make sure we have a session slot for this...
    auto session = sid->getSession();
    if(!session) {
        session = new SessionItem(sid);
        model->add(session);
    }

    auto msg = new MessageItem(session, from, to, text, timestamp, sequence, subject);
    if(!msg->isValid()) {
        qDebug() << "Probably duplicate message";
        delete msg;
        return;
    }

    // if session still unloaded, only create on disk...
    if(session->isLoaded() || activeItem == session)
        session->addMessage(msg);
    else
        delete msg;

    if(activeItem == session) {
        bool bottom = true;
        auto scroll = ui.messages->verticalScrollBar();
        if(scroll && scroll->value() != scroll->maximum())
            bottom = false;
        if(bottom)
            scrollToBottom();
        else {
            session->addUnread();
            ui.bottom->setVisible(true);
        }
    }
    else {
        session->addUnread();
    }
}

void Sessions::checkInput(const QString& text)
{
    // Validate < 160 for utf8, since is how we send
    // In future may also validate ucs2...

    if(text.toUtf8().length() > 155) {
        ui.input->setText(text.left(text.length() - 1));
        desktop->warningMessage("Maximum input length reached", 500);
    }
}

void Sessions::scrollToBottom()
{
    ui.bottom->setVisible(false);
    if(!activeItem || activeItem->count() < 1)
        return;
    activeItem->clearUnread();
    auto index = activeItem->lastMessage();
    ui.messages->scrollTo(index, QAbstractItemView::PositionAtBottom);
}

void Sessions::finishInput(const QString& error, const QDateTime& timestamp, int sequence)
{
    if(!error.isEmpty())
        desktop->errorMessage(error);
    else if(inputItem) {
        Q_ASSERT(inputItem->messageModel != nullptr);
        inputItem->addMessage(new MessageItem(inputItem, inputItem->text(), timestamp, sequence));
        inputItem->setText("");
        if(inputItem == activeItem) {
            bool bottom = true;
            auto scroll = ui.messages->verticalScrollBar();
            if(scroll && scroll->value() != scroll->maximum())
                bottom = false;
            ui.input->setText("");
            if(bottom)
                scrollToBottom();
            else {
                inputItem->addUnread();
                ui.bottom->setVisible(true);
            }
        }
        else {
            inputItem->addUnread();
        }
    }

    inputItem = nullptr;
    ui.input->setEnabled(true);
    if(activeItem)
        ui.input->setFocus();
    else
        ui.messages->setFocus();
}






