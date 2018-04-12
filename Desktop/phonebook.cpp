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
static bool mousePressed = false;
static QPoint mousePosition;

Phonebook *Phonebook::Instance = nullptr;
QList<ContactItem *> ContactItem::users;
QList<ContactItem *> ContactItem::groups;
QHash<UString,ContactItem *> ContactItem::foreign;
QHash<int,ContactItem *> ContactItem::index;
QSet<QString> ContactItem::usrAuths;
QSet<QString> ContactItem::grpAuths;

ContactItem::ContactItem(const QSqlRecord& record)
{
    session = nullptr;
    group = false;
    added = false;

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
    extendedInfo = record.value("info").toString();
    uid = record.value("uid").toInt();
    index[uid] = this;
}

void ContactItem::add()
{
    Q_ASSERT(!added);
    added = true;
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

    qDebug() << "NUMBER" << textNumber << "NAME" << displayName << "URI" << contactUri << "TYPE" << contactType << group;

    if(!isExtension() || extensionNumber > 999) {
        contactType = "FOREIGN";
        foreign[contactUri] = this;
        return;
    }

    if(contactType == "USER" || contactType == "DEVICE")
        usrAuths << authUserId;
    else
        grpAuths << authUserId;

    if(displayName.isEmpty())
        return;

    contactFilter = (textNumber + "\n" + displayName).toLower();
    if(extensionNumber > highest)
        highest = extensionNumber;

    local[extensionNumber] = this;
}

void ContactItem::purge()
{
    foreach(auto item, index.values()) {
        Q_ASSERT(item->added);
        delete item;
    }

    foreign.clear();
    groups.clear();
    users.clear();
    usrAuths.clear();
    grpAuths.clear();
    index.clear();
    memset(local, 0, sizeof(local));
    highest = -1;
    me = -1;

    grpAuths << "anonymous";
    grpAuths << "system";
}

QStringList ContactItem::allUsers()
{
    auto list = usrAuths.toList();
    list.sort();
    return list;
}

QStringList ContactItem::allGroups()
{
    auto list = grpAuths.toList();
    list.sort();
    return list;
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

ContactItem *LocalContacts::updateContact(const QJsonObject& json)
{
    auto number = json["n"].toInt();
    if(number < 0 || number > 1000)
        return nullptr;

    auto storage = Storage::instance();
    if(!storage)
        return nullptr;

    auto status = json["s"].toString();
    auto type = json["t"].toString();
    auto display = json["d"].toString();
    auto uri = json["u"].toString();
    auto user = json["a"].toString();
    auto puburi = json["p"].toString();
    auto mailto = json["e"].toString();
    auto info = json["i"].toString();
    auto insert = false;
    auto item = local[number];

    if(item && !status.isEmpty()) {
        if((type == "USER" || type == "DEVICE") && Desktop::isAdmin() && item != Phonebook::self()) {
            if(status == "sysadmin")
                ui.adminButton->setText(tr("Disable"));
            else
                ui.adminButton->setText(tr("Enable"));
            if(status == "suspend") {
                ui.suspendButton->setText(tr("Restore"));
                ui.adminButton->setEnabled(false);
            }
            else {
                ui.suspendButton->setText(tr("Suspend"));
                ui.adminButton->setEnabled(true);
            }
            ui.profileStack->setCurrentWidget(ui.adminPage);
        }
    }

    if(number > highest) {
        insert = true;
        beginInsertRows(QModelIndex(), highest, number);
        highest = number;
    }

    if(!item) {
        storage->runQuery("INSERT INTO Contacts(extension, dialing, type, display, user, uri, mailto, puburi, info) VALUES(?,?,?,?,?,?,?,?,?);", {
                              number, QString::number(number), type, display, user, uri, mailto, puburi, info});
        auto query = storage->getRecords("SELECT * FROM Contacts WHERE extension=?;", {
                                             number});
        if(query.isActive() && query.next()) {
            local[number] = item = new ContactItem(query.record());
            item->add();
        }
    }
    else {
        auto old = item->displayName;
        if(json["i"].isNull())
            info = item->info();
        else
            item->extendedInfo = info;

        item->contactFilter = (item->textNumber + "\n" + item->displayName).toLower();
        item->textDisplay = display;
        item->contactEmail = mailto.toUtf8();
        item->displayName = display.toUtf8();
        item->contactUri = uri.toUtf8();
        item->contactPublic = puburi.toUtf8();
        item->authUserId = user.toLower();
        storage->runQuery("UPDATE Contacts SET display=?, user=?, uri=?, mailto=?, puburi=?, info=? WHERE extension=?;", {
                              display, user, uri, mailto, puburi, info, number});
        if(old == item->displayName)
            return item;

        if(item->session)
            SessionModel::update(item->session);
        else if(item == Phonebook::self())
            Desktop::instance()->setSelf(QString::fromUtf8(item->displayName));
    }

    if(insert)
        endInsertRows();
    else
        dataChanged(index(number), index(number));
    return item;
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
    int width = style.rect.width() - 32;
    auto metrics = painter->fontMetrics();
    auto text = metrics.elidedText(item->textDisplay, Qt::ElideRight, width);
    painter->drawText(pos, text);
}

Phonebook::Phonebook(Desktop *control, Sessions *sessions) :
QWidget(), desktop(control), localModel(nullptr), connector(nullptr), refreshRoster()
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;
    requestPending = false;

    ui.setupUi(static_cast<QWidget *>(this));
    ui.contacts->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui.contact->setVisible(false);

    connect(Toolbar::search(), &QLineEdit::returnPressed, this, &Phonebook::search);
    connect(Toolbar::search(), &QLineEdit::textEdited, this, &Phonebook::filterView);
    connect(desktop, &Desktop::changeStorage, this, &Phonebook::changeStorage);
    connect(desktop, &Desktop::changeConnector, this, &Phonebook::changeConnector);
    connect(ui.contacts, &QListView::clicked, this, &Phonebook::selectContact);
    connect(ui.displayName, &QLineEdit::returnPressed, this, &Phonebook::changeProfile);
    connect(ui.emailAddress, &QLineEdit::returnPressed, this, &Phonebook::changeProfile);
    connect(ui.world, &QPushButton::pressed, this, &Phonebook::promoteRemote);
    connect(ui.noworld, &QPushButton::pressed, this, &Phonebook::demoteLocal);
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

    connect(ui.sessionButton, &QPushButton::pressed, this, [=] {
        if(activeItem)
            sessions->activateContact(activeItem);
    });

    connect(&refreshRoster, &QTimer::timeout, this, [this] {
       if(connector)
           connector->requestRoster();
    });

    ContactItem::purge();
    localPainter = new LocalDelegate(this);
    ui.contacts->setItemDelegate(localPainter);
}

void Phonebook::setWidth(int width)
{
    if(width < 132)
        width = 132;
    if(width > size().width() / 3)
        width = size().width() / 3;
    ui.contacts->setMaximumWidth(width);
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
    Toolbar::setTitle("");
    localModel->setFilter(selected);
    ui.contact->setVisible(false);
    ui.contacts->scrollToTop();
}

void Phonebook::promoteRemote()
{
    if(!activeItem)
        return;

    if(activeItem->number() < 1 || activeItem->number() > 999) {
        desktop->errorMessage(tr("Cannot modify this contact"));
        ui.displayName->setText(activeItem->display());
        ui.emailAddress->setText(activeItem->mailTo());
        return;
    }

    if(!connector) {
        desktop->errorMessage(tr("Cannot modify while offline"));
        return;
    }

    desktop->statusMessage(tr("updating profile"));
    QJsonObject json = {
        {"a", "remote"},
    };
    QJsonDocument jdoc(json);
    auto body = jdoc.toJson(QJsonDocument::Compact);

    connector->sendProfile(activeItem->uri(), body);
    ui.displayName->setEnabled(false);
    ui.emailAddress->setEnabled(false);
    ui.world->setEnabled(false);
    ui.noworld->setEnabled(false);
    ui.behaviorGroup->setEnabled(false);
    ui.forwardGroup->setEnabled(false);
    ui.profileStack->setEnabled(false);
}

void Phonebook::demoteLocal()
{
    if(!activeItem)
        return;

    if(activeItem->number() < 1 || activeItem->number() > 999) {
        desktop->errorMessage(tr("Cannot modify this contact"));
        ui.displayName->setText(activeItem->display());
        ui.emailAddress->setText(activeItem->mailTo());
        return;
    }

    if(!connector) {
        desktop->errorMessage(tr("Cannot modify while offline"));
        return;
    }

    desktop->statusMessage(tr("updating profile"));
    QJsonObject json = {
        {"a", "local"},
    };
    QJsonDocument jdoc(json);
    auto body = jdoc.toJson(QJsonDocument::Compact);

    connector->sendProfile(activeItem->uri(), body);
    ui.displayName->setEnabled(false);
    ui.emailAddress->setEnabled(false);
    ui.world->setEnabled(false);
    ui.noworld->setEnabled(false);
    ui.behaviorGroup->setEnabled(false);
    ui.forwardGroup->setEnabled(false);
    ui.profileStack->setEnabled(false);
}

void Phonebook::changeProfile()
{
    if(!activeItem)
        return;

    if(activeItem->number() < 1 || activeItem->number() > 999) {
        desktop->errorMessage(tr("Cannot modify this contact"));
        ui.displayName->setText(activeItem->display());
        ui.emailAddress->setText(activeItem->mailTo());
        return;
    }

    if(!connector) {
        desktop->errorMessage(tr("Cannot modify while offline"));
        return;
    }

    desktop->statusMessage(tr("updating profile"));
    QJsonObject json = {
        {"d", ui.displayName->text().trimmed()},
        {"e", ui.emailAddress->text().trimmed()},
        {"n", activeItem->number()},
    };
    QJsonDocument jdoc(json);
    auto body = jdoc.toJson(QJsonDocument::Compact);

    connector->sendProfile(activeItem->uri(), body);
    ui.displayName->setEnabled(false);
    ui.emailAddress->setEnabled(false);
    ui.world->setEnabled(false);
    ui.noworld->setEnabled(false);
    ui.behaviorGroup->setEnabled(false);
    ui.forwardGroup->setEnabled(false);
    ui.profileStack->setEnabled(false);
}

void Phonebook::updateProfile(ContactItem *item)
{       
    if(!item || item != activeItem)
        return;

    auto type = item->type().toLower();
    ui.displayName->setText(item->display());
    ui.emailAddress->setText(item->mailTo());
    if(type == "group" || type == "system")
        ui.avatarButton->setIcon(QIcon(":/icons/group.png"));
    else if(type == "device" || type == "phone")
        ui.avatarButton->setIcon(QIcon(":/icons/phone.png"));
    else
        ui.avatarButton->setIcon(QIcon(":/icons/person.png"));

    QString uri = QString::fromUtf8(item->publicUri());
    if(type == "system" || type == "device") {
        ui.world->setVisible(false);
        ui.noworld->setVisible(false);
        ui.uri->setText(QString::fromUtf8(item->uri()));
    }
    else if(uri.length() > 0) {
        ui.world->setVisible(false);
        ui.noworld->setVisible(true);
        ui.uri->setText("sip:" + uri);
    }
    else {
        ui.world->setVisible(true);
        ui.noworld->setVisible(false);
        ui.uri->setText(QString::fromUtf8(item->uri()));
    }
    if(item == self() || desktop->isAdmin() || desktop->isOperator()) {
        ui.displayName->setEnabled(true);
        ui.displayName->setFocus();
    }
    else
        ui.displayName->setEnabled(false);
    if(item == self() || desktop->isAdmin()) {
        ui.emailAddress->setEnabled(true);
        ui.forwardGroup->setEnabled(true);
    }
    else {
        ui.emailAddress->setEnabled(false);
        ui.forwardGroup->setEnabled(false);
    }

    if(desktop->isAdmin()) {
        ui.world->setEnabled(true);
        ui.noworld->setEnabled(true);
    }
    else {
        ui.world->setEnabled(false);
        ui.noworld->setEnabled(false);
    }

    ui.profileStack->setEnabled(true);
    if(item == self())
        ui.behaviorGroup->setEnabled(false);
    else
        ui.behaviorGroup->setEnabled(true);
}

void Phonebook::selectContact(const QModelIndex& index)
{
    ui.profileStack->setCurrentWidget(ui.blankPage);
    auto row = index.row();
    if(row < 0 || row > highest)
        return;

    auto item = local[row];
    if(!item)
        return;

    activeItem = clickedItem = item;
    updateProfile(item);
    localModel->clickContact(row);
    if(connector && activeItem->number() > 0) {
        // make sure we have current profile...
        desktop->statusMessage(tr("Updating profile"));
        connector->requestProfile(activeItem->uri());
        ui.displayName->setEnabled(false);
        ui.emailAddress->setEnabled(false);
        ui.world->setEnabled(false);
        ui.noworld->setEnabled(false);
    }
    else
        desktop->clearMessage();

    // delay to avoid false view if double-click activation
    QTimer::singleShot(CONST_CLICKTIME, this, [=] {
        if(highest > -1 && row <= highest) {
            localModel->clickContact(row);
            ui.contact->setVisible(true);
            auto type = activeItem->type().toLower();
            auto user = activeItem->user();
            auto number = QString::number(activeItem->number());
            if(!activeItem->isExtension())
                Toolbar::setTitle(activeItem->dialed());
            else if(type == "system" || type == "group")
                Toolbar::setTitle("#" + number + " \"" + user + "\"");
            else
                Toolbar::setTitle("@" + number + " \"" + user + "\"");
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
    requestPending = false;

    if(connector) {
        requestPending = true;
        connect(connector, &Connector::changeProfile, this, [=](const QByteArray& json) {
            if(!connector)
                return;

            auto jdoc = QJsonDocument::fromJson(json);
            desktop->clearMessage();
            updateProfile(localModel->updateContact(jdoc.object()));
        }, Qt::QueuedConnection);

        connect(connector, &Connector::changeRoster, this, [=](const QByteArray& json) {
            if(!connector)
                return;

            desktop->clearRoster();
            refreshRoster.start(3600000);   // once an hour...

            auto jdoc = QJsonDocument::fromJson(json);
            auto list = jdoc.array();

            foreach(auto profile, list) {
                updateProfile(localModel->updateContact(profile.toObject()));
            }

            if(requestPending) {
                connector->requestPending();
                requestPending = false;
            }
        }, Qt::QueuedConnection);

        if(desktop->checkRoster()) {
            QTimer::singleShot(300, this, [this]{
                if(connector)
                    connector->requestRoster();
            });
        }
        else {
            requestPending = false;
            QTimer::singleShot(300, this, [this] {
                if(connector)
                    connector->requestPending();
            });
        }
    }
    else {
        ui.profileStack->setCurrentWidget(ui.blankPage);
        refreshRoster.stop();
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
            while(query.next()) {
                auto item = new ContactItem(query.record());
                item->add();
            }
            localModel = new LocalContacts(this);
        }
        emit changeSessions(storage, ContactItem::sessions());
    }

    ui.contacts->setModel(localModel);
}

bool Phonebook::event(QEvent *event)
{
    switch(event->type()) {
    case QEvent::MouseButtonPress: {
        auto mpos = static_cast<QMouseEvent *>(event)->pos();
        auto rect = QRect(ui.separator->pos(), ui.separator->size());
        rect.setLeft(rect.left() - 2);
        rect.setRight(rect.right() + 2);
        if(rect.contains(mpos)) {
            mousePosition = mpos;
            mousePressed = true;
            ui.separator->setStyleSheet("color: red;");
        }
        break;
    }
    case QEvent::MouseMove: {
        if(mousePressed) {
            auto mpos = static_cast<QMouseEvent *>(event)->pos();
            auto width = ui.contacts->maximumWidth() + mpos.x() - mousePosition.x();
            auto maxWidth = size().width() / 3;
            if(width < 132 || width > maxWidth) {
                mousePressed = false;
                ui.separator->setStyleSheet(QString());
                break;
            }
            mousePosition.setX(mpos.x());
            ui.contacts->setMaximumWidth(width);
            emit changeWidth(width);
            if(localModel)
                localModel->changeLayout();
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


