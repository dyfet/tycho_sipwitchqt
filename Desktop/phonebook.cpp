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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static Ui::PhonebookWindow ui;
static int highest = -1, me = -1;
static ContactItem *local[1000];
static UString searching;
static ContactItem *activeItem = nullptr;
static ContactItem *clickedItem = nullptr;
static bool initialRoster = false;

QList<ContactItem *> ContactItem::users;
QList<ContactItem *> ContactItem::groups;
QHash<UString,ContactItem *> ContactItem::foreign;
QHash<int,ContactItem *> ContactItem::index;

ContactItem::ContactItem(const QSqlRecord& record)
{
    session = nullptr;
    group = false;

    extensionNumber = record.value("extension").toInt();
    contactUpdated = record.value("last").toDateTime();
    contactTimestamp = contactUpdated.toString(Qt::ISODate);
    contactType = record.value("type").toString();
    contactUri = record.value("uri").toString().toUtf8();
    contactPublic = record.value("puburi").toString().toUtf8();
    contactEmail = record.value("mailto").toString().toUtf8();
    displayName = record.value("display").toString().toUtf8();
    textNumber = record.value("dialing").toString();
    textDisplay = record.value("display").toString();
    authUserId = record.value("user").toString().toLower();
    uid = record.value("uid").toInt();
    index[uid] = this;

    // the subset of contacts that have active sessions at load time...
    qDebug() << "***** UPDATED" << contactUpdated << contactUpdated.isValid();
    if(contactUpdated.isValid()) {
        if(contactType == "GROUP") {
            group = true;
            groups << this;
        }
        else if(me != extensionNumber) {
            users << this;
        }
    }

    if(contactType == "GROUP")
        group = true;

    if(contactType == "SYSTEM" && extensionNumber == 0) {
        group = true;
        groups.insert(0, this);
    }

    qDebug() << "NUMBER" << textNumber << "NAME" << displayName << "URI" << contactUri << "TYPE" << contactType << "DATE" << record.value("last").toULongLong() << group;

    if(!isExtension() || extensionNumber > 999) {
        contactType = "FOREIGN";
        foreign[contactUri] = this;
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
    foreach(auto item, index.values())
        delete item;

    foreign.clear();
    groups.clear();
    users.clear();
    index.clear();
    memset(local, 0, sizeof(local));
    highest = -1;
    me = -1;
}

ContactItem *ContactItem::find(const UString& id)
{
    if(id.isNumber())
        return findExtension(id.toInt());

    if(id.indexOf('@') < 1)
        return nullptr;

    // we should create new phonebook entries for "discovered" contacts...
    return foreign[id];
}

ContactItem *ContactItem::findExtension(int number)
{
    if(number >= 0 && number < 1000)
        return local[number];

    return nullptr;
}

ContactItem *ContactItem::findText(const QString& text)
{
    auto match = text.toLower();
    unsigned matches = 0;
    ContactItem *found = nullptr;

    auto item = index.begin();
    while(item != index.end()) {
        if((*item)->contactFilter.indexOf(text) > -1 || (*item)->authUserId == text) {
            if(++matches > 1) {
                found = nullptr;
                break;
            }
            found = *item;
        }
        ++item;
    }
    return found;
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

void LocalContacts::clickContact(int row)
{
    if(row < 0 || row > highest)
        return;

    dataChanged(index(row), index(row));
}

void LocalContacts::updateContact(const QJsonObject& json)
{
    auto number = json["n"].toInt();
    if(number < 0 || number > 1000)
        return;

    auto storage = Storage::instance();
    if(!storage)
        return;

    auto type = json["t"].toString();
    auto display = json["d"].toString();
    auto uri = json["u"].toString();
    auto user = json["a"].toString();
    auto puburi = json["p"].toString();
    auto mailto = json["e"].toString();

    auto insert = false;
    auto item = local[number];

    if(number > highest) {
        insert = true;
        beginInsertRows(QModelIndex(), highest, number);
        highest = number;
    }

    if(!item) {
        storage->runQuery("INSERT INTO Contacts(extension, dialing, type, display, user, uri, mailto, puburi) VALUES(?,?,?,?,?,?,?,?);", {
                              number, QString::number(number), type, display, user, uri, mailto, puburi});
        auto query = storage->getRecords("SELECT * FROM Contacts WHERE extension=?;", {
                                             number});
        if(query.isActive() && query.next()) {
            local[number] = new ContactItem(query.record());
        }
    }
    else {
        auto old = item->displayName;
        item->contactFilter = (item->textNumber + "\n" + item->displayName).toLower();
        item->textDisplay = display;
        item->displayName = display.toUtf8();
        item->contactUri = uri.toUtf8();
        item->authUserId = user.toLower();
        storage->runQuery("UPDATE Contacts SET display=?, user=?, uri=?, mailto=?, puburi=? WHERE extension=?;", {
                              display, user, uri, number, mailto, puburi});
        if(old == item->displayName)
            return;

        if(item->session)
            SessionModel::update(item->session);
    }

    if(insert)
        endInsertRows();
    else
        dataChanged(index(number), index(number));
}

QSize LocalDelegate::sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    Q_UNUSED(style);

    auto row = index.row();
    auto rows = highest + 1;

    if(row < 0 || row >= rows)
        return {0, 0};

    auto item = local[row];
    if(item == nullptr || item->display().isEmpty())
        return {0, 0};

    auto filter = item->filter();
    if(searching.length() > 0 && filter.length() > 0) {
        if(filter.indexOf(searching) < 0 && searching != item->authUserId)
            return {0, 0};
    }

    return {ui.contacts->width(), CONST_CELLHIGHT};
}

void LocalDelegate::paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    if(style.rect.isEmpty())
        return;

    auto item = local[index.row()];
    auto pos = style.rect.bottomLeft();

    if(item == clickedItem) {
        painter->fillRect(style.rect, QColor(CONST_CLICKCOLOR));
        clickedItem = nullptr;
    }

    pos.ry() -= CONST_CELLLIFT;  // off bottom
    painter->drawText(pos, item->textNumber);
    pos.rx() += 32;
    painter->drawText(pos, item->textDisplay);
}

Phonebook::Phonebook(Desktop *control, Sessions *sessions) :
QWidget(), desktop(control), localModel(nullptr), connector(nullptr), refreshRoster()
{
    ui.setupUi(static_cast<QWidget *>(this));
    ui.contacts->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui.contact->setVisible(false);

    connect(Toolbar::search(), &QLineEdit::returnPressed, this, &Phonebook::search);
    connect(Toolbar::search(), &QLineEdit::textEdited, this, &Phonebook::filterView);
    connect(desktop, &Desktop::changeStorage, this, &Phonebook::changeStorage);
    connect(desktop, &Desktop::changeConnector, this, &Phonebook::changeConnector);
    connect(ui.contacts, &QListView::clicked, this, &Phonebook::selectContact);
    connect(this, &Phonebook::changeSessions, sessions, &Sessions::changeSessions);

    connect(ui.contacts, &QListView::doubleClicked, this, [=](const QModelIndex& index){
        auto row = index.row();
        if(row < 0 || row > highest)
            return;

        auto item = local[row];
        if(!item)
            return;

        sessions->activateContact(item);
    });

    connect(&refreshRoster, &QTimer::timeout, this, [this]{
       if(connector)
           connector->requestRoster();
    });

    ContactItem::purge();
    localPainter = new LocalDelegate(this);
    ui.contacts->setItemDelegate(localPainter);
}

void Phonebook::enter()
{
    Toolbar::setTitle("");
    Toolbar::search()->setEnabled(true);
    Toolbar::search()->setText("");
    Toolbar::search()->setPlaceholderText(tr("Filter by number or name"));
    Toolbar::search()->setFocus();
    filterView("");
    ui.contact->setVisible(false);
}

void Phonebook::search()
{
    if(!desktop->isCurrent(this))
        return;
}

void Phonebook::filterView(const QString& selected)
{
    if(!desktop->isCurrent(this) || !localModel)
        return;

    if(selected == searching)
        return;

    activeItem = nullptr;
    localModel->setFilter(selected);
    ui.contact->setVisible(false);
    ui.contacts->scrollToTop();
}

void Phonebook::selectContact(const QModelIndex& index)
{
    auto row = index.row();
    if(row < 0 || row > highest)
        return;

    auto item = local[row];
    if(!item)
        return;

    activeItem = clickedItem = item;
    ui.displayName->setText(item->display());
    ui.type->setText(item->type().toLower());
    ui.uri->setText(item->uri());
    if(item->publicUri().isEmpty())
        ui.authorizingUser->setText(item->user());
    else
        ui.authorizingUser->setText(item->publicUri());
    localModel->clickContact(row);

    // delay to avoid false view if double-click activation
    QTimer::singleShot(CONST_CLICKTIME, this, [=] {
        if(highest > -1 && row <= highest) {
            localModel->clickContact(row);
            ui.contact->setVisible(true);
        }
    });
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
    if(!connector)
        refreshRoster.stop();

    if(connector) {
        connect(connector, &Connector::changeRoster, this, [=](const QByteArray& json) {
            if(!connector)
                return;

            initialRoster = true;
            refreshRoster.start(3600000);   // once an hour...

            auto jdoc = QJsonDocument::fromJson(json);
            auto list = jdoc.array();

            foreach(auto profile, list) {
                localModel->updateContact(profile.toObject());
            }

            connector->requestPending();
        }, Qt::QueuedConnection);

        if(!initialRoster) {
            QTimer::singleShot(300, this, [this]{
                if(connector)
                    connector->requestRoster();
            });
        }
        else {
            QTimer::singleShot(300, this, [this] {
                if(connector)
                    connector->requestPending();
            });
        }
    }
}

void Phonebook::changeStorage(Storage *storage)
{
    if(localModel) {
        delete localModel;
        localModel = nullptr;
    }

    ContactItem::purge();

    if(storage) {
        auto creds = storage->credentials();
        me = creds["extension"].toInt();
        // load contacts in last update time order...
        auto query = storage->getRecords("SELECT * FROM Contacts ORDER BY last DESC, sequence DESC");
        if(query.isActive()) {
            qDebug() << "***** LOADING CONTACTS, SELF=" << me;
            while(query.next()) {                               // NOLINT
                auto item = new ContactItem(query.record());    // NOLINT
                Q_UNUSED(item);
            }
            localModel = new LocalContacts(this);
        }
        emit changeSessions(storage, ContactItem::sessions());
    }
    else
        initialRoster = false;

    ui.contacts->setModel(localModel);
}


