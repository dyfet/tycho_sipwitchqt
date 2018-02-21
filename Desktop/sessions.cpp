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

#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif

static Ui::SessionsWindow ui;
static QList<SessionItem *> sessions;
static SessionItem *active, *local[1000];
static unsigned groups = 0; // size of group section...
static QString userStatus = "@", groupStatus = "#";
static QPen groupActive("blue"), groupDefault("black");
static QPen onlineUser("green"), offlineUser("black"), busyUser("yellow");

SessionItem::SessionItem(ContactItem *contactItem, bool active)
{
    contact = contactItem;

    auto type = contactItem->type();
    if(type == "SYSTEM" || type == "GROUP") {
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
    busy = false;

    auto number = contactItem->number();
    if(number > -1 && number < 1000)
        local[number] = this;
    sessions << this;
}

bool SessionItem::setOnline(bool flag)
{
    std::swap(flag, online);
    if(!online)
        busy = false;
    return flag;
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
    groups = 0;
    memset(local, 0, sizeof(local));
    foreach(auto session, sessions) {
        delete session;
    }
    sessions.clear();
}

QSize SessionDelegate::sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    Q_UNUSED(style);

    auto row = index.row();
    auto rows = sessions.size();

    if(row < 0 || row >= rows)
        return QSize(0,0);

    auto item = sessions[row];
    auto contact = item->contact;
    if(contact == nullptr || item->display().isEmpty())
        return QSize(0, 0);

    return QSize(ui.sessions->width(), 16);
}

void SessionDelegate::paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    if(style.rect.isEmpty())
        return;

    auto pen = painter->pen();
    auto item = sessions[index.row()];
    auto pos = style.rect.bottomLeft();
    QColor color("black");

    painter->setPen(item->color);
    painter->drawText(pos, item->status);

    pos.rx() += 16;
    painter->setPen(pen);
    painter->drawText(pos, item->display());
}

Sessions::Sessions(Desktop *control) :
QWidget(), desktop(control), model(nullptr)
{
    ui.setupUi(static_cast<QWidget *>(this));
    ui.sessions->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui.messages->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(Toolbar::search(), &QLineEdit::returnPressed, this, &Sessions::search);
    connect(desktop, &Desktop::changeStorage, this, &Sessions::changeStorage);
    connect(desktop, &Desktop::changeSessions, this, &Sessions::changeSessions);
    connect(desktop, &Desktop::changeConnector, this, &Sessions::changeConnector);

    SessionModel::purge();
    sessions = new SessionDelegate(this);
    ui.sessions->setItemDelegate(sessions);
}

void Sessions::enter()
{
    Toolbar::search()->setEnabled(true);
    ui.input->setFocus();
}

void Sessions::search()
{
    if(!desktop->isCurrent(this))
        return;
}

void Sessions::changeStorage(Storage *storage)
{
    if(storage == nullptr) {
        SessionModel::purge();
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

    SessionModel::purge();
    foreach(auto contact, contacts) {
        auto item = new SessionItem(contact);
        if(item->type() == "GROUP" || item->type() == "SYSTEM")
            ++groups;
        model = new SessionModel(this);
    }
    ui.sessions->setModel(model);
    qDebug() << groups << "GROUPS LOADED";
}

void Sessions::changeConnector(Connector *connected)
{
    connector = connected;
    if(connector)
        ui.status->setStyleSheet("color: green;");
    else
        ui.status->setStyleSheet("color: black;");
}
