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
#include "ui_phonebook.h"

#include <QPainter>

static Ui::PhonebookWindow ui;
static int highest = -1, me = -1;
static ContactItem *local[1000];
static UString searching;
static ContactItem *activeItem = nullptr;

ContactItem *ContactItem::list = nullptr;
QList<ContactItem *> ContactItem::users;
QList<ContactItem *> ContactItem::groups;

ContactItem::ContactItem(const QSqlRecord& record)
{
    prior = list;
    list = this;

    extensionNumber = record.value("extension").toInt();
    contactUpdated = record.value("last").toDateTime();
    contactTimestamp = contactUpdated.toString(Qt::ISODate);
    contactType = record.value("type").toString();
    contactUri = record.value("uri").toString().toUtf8();
    displayName = record.value("display").toString().toUtf8();
    textNumber = record.value("dialing").toString();
    textDisplay = record.value("display").toString();
    uid = record.value("uid").toInt();

    // the subset of contacts that have active sessions at load time...
    if(record.value("last").toULongLong() > 0) {
        if(contactType == "GROUP") {
            groups << this;
        }
        else if(me != extensionNumber) {
            users << this;
        }
    }

    if(contactType == "SYSTEM" && extensionNumber == 0)
        groups.insert(0, this);

    qDebug() << "NUMBER" << textNumber << "NAME" << displayName << "URI" << contactUri << "TYPE" << contactType << "DATE" << record.value("last").toULongLong();

    if(!isExtension() || extensionNumber > 999) {
        contactType = "FOREIGN";
        return;
    }

    if(displayName.isEmpty())
        return;

    contactFilter = (textNumber + "\n" + displayName).toLower();
    if(extensionNumber > highest)
        highest = extensionNumber;

    local[extensionNumber] = this;
}

void ContactItem::purge()
{
    while(list != nullptr) {
        ContactItem *next = list->prior;
        delete list;
        list = next;
    }
    groups.clear();
    users.clear();
    memset(local, 0, sizeof(local));
    highest = -1;
    me = -1;
    list = nullptr;
}

int LocalContacts::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    if(highest < 0)
        return 0;
    return highest + 1;
}

QVariant LocalContacts::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    int rows = rowCount(index);
    if(row < 0 || row >= rows || role!= Qt::DisplayRole)
        return QVariant();

    auto item = local[row];
    if(item == nullptr || item->display().isEmpty())
        return QVariant();

    return QString::number(row) + " " + item->display();
}

void LocalContacts::setFilter(const UString& filter)
{
    emit layoutAboutToBeChanged();
    searching = filter.toLower();
    emit layoutChanged();
}

QSize LocalDelegate::sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    Q_UNUSED(style);

    auto row = index.row();
    auto rows = highest + 1;

    if(row < 0 || row >= rows)
        return QSize(0,0);

    auto item = local[row];
    if(item == nullptr || item->display().isEmpty())
        return QSize(0, 0);

    auto filter = item->filter();
    if(searching.length() > 0 && filter.length() > 0) {
        if(filter.indexOf(searching) < 0)
            return QSize(0, 0);
    }

    return QSize(ui.contacts->width(), 16);
}

void LocalDelegate::paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    if(style.rect.isEmpty())
        return;

    auto item = local[index.row()];
    auto pos = style.rect.bottomLeft();

    painter->drawText(pos, item->textNumber);
    pos.rx() += 32;
    painter->drawText(pos, item->textDisplay);
}

Phonebook::Phonebook(Desktop *control) :
QWidget(), desktop(control), localModel(nullptr), connector(nullptr)
{
    ui.setupUi(static_cast<QWidget *>(this));
    ui.contacts->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui.contact->setVisible(false);

    connect(Toolbar::search(), &QLineEdit::returnPressed, this, &Phonebook::search);
    connect(Toolbar::search(), &QLineEdit::textEdited, this, &Phonebook::filter);
    connect(desktop, &Desktop::changeStorage, this, &Phonebook::changeStorage);
    connect(desktop, &Desktop::changeConnector, this, &Phonebook::changeConnector);
    connect(ui.contacts, &QListView::clicked, this, &Phonebook::select);

    ContactItem::purge();
    localPainter = new LocalDelegate(this);
    ui.contacts->setItemDelegate(localPainter);
}

void Phonebook::enter()
{
    Toolbar::search()->setEnabled(true);
    Toolbar::search()->setText("");
    Toolbar::search()->setFocus();
    filter("");
    ui.contact->setVisible(false);
}

void Phonebook::search()
{
    if(!desktop->isCurrent(this))
        return;
}

void Phonebook::filter(const QString& selected)
{
    if(!desktop->isCurrent(this) && !localModel)
        return;

    if(selected == searching)
        return;

    activeItem = nullptr;
    localModel->setFilter(selected);
    ui.contact->setVisible(false);
    ui.contacts->scrollToTop();
}

void Phonebook::select(const QModelIndex& index)
{
    auto row = index.row();
    if(row < 0 || row > highest)
        return;

    auto item = local[row];
    if(!item)
        return;

    activeItem = item;
    ui.displayName->setText(item->display());
    ui.type->setText(item->type().toLower());
    ui.uri->setText(item->uri());
    ui.contact->setVisible(true);
}

ContactItem *Phonebook::self()
{
    if(me < 1)
        return nullptr;

    return local[me];
}

void Phonebook::changeConnector(Connector *connected)
{
    connector = connected;
    // connects to be added for contact events...
}

void Phonebook::changeStorage(Storage *storage)
{
    if(localModel) {
        delete localModel;
        localModel = nullptr;
    }

    ContactItem::purge();

    if(storage) {
        memset(local, 0, sizeof(local));
        highest = -1;
        auto creds = storage->credentials();
        me = creds["extension"].toInt();
        // load contacts in last update time order...
        auto query = storage->getRecords("SELECT * FROM Contacts ORDER BY last, sequence DESC");
        if(query.isActive()) {
            qDebug() << "***** LOADING CONTACTS, SELF=" << me;
            while(query.next()) {
                auto item = new ContactItem(query.record());
                Q_UNUSED(item);
            }
            localModel = new LocalContacts(this);
        }
    }
    ui.contacts->setModel(localModel);
    desktop->activateSessions(ContactItem::sessions());
}


