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
static int highest = 699, me = -1, removes = 0;
static ContactItem *local[1000];
static UString searching;
static ContactItem *activeItem = nullptr;
static ContactItem *clickedItem = nullptr;
static bool mousePressed = false;
static QPoint mousePosition;
static int cellHeight, cellLift = 3;
static QIcon addIcon, delIcon, newIcon;
static MemberModel *memberModel = nullptr;

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
    group = added = hidden = admin = online = suspended = false;

    extensionNumber = record.value("extension").toInt();
    contactCreated = record.value("sync").toDateTime();
    contactUpdated = record.value("last").toDateTime();
    contactTimestamp = contactUpdated.toString(Qt::ISODate);
    contactSequence = record.value("sequence").toInt();
    contactTopic = record.value("topic").toString();
    contactType = record.value("type").toString();
    contactUri = record.value("uri").toString().toUtf8();
    contactPublic = record.value("puburi").toString().toUtf8();
    contactEmail = record.value("mailto").toString().toUtf8();
    displayName = record.value("display").toString().toUtf8();
    textNumber = record.value("dialing").toString();
    textDisplay = record.value("display").toString();
    authUserId = record.value("user").toString().toLower();
    extendedInfo = record.value("info").toString();
    publicKey = record.value("pubkey").toByteArray();
    topicUpdated = record.value("tsync").toDateTime();
    topicSequence = record.value("tseq").toInt();
    uid = record.value("uid").toInt();
    index[uid] = this;
}

void ContactItem::add()
{
    Q_ASSERT(!added);
    added = true;
    // the subset of contacts that have active sessions at load time...
    if(contactUpdated.isValid()) {
        if(contactType == "GROUP" || contactType == "PILOT") {
            group = true;
            groups << this;
        }
        else if(me != extensionNumber) {
            users << this;
        }
    }

    if(contactType == "GROUP" || contactType == "PILOT")
        group = true;

    if(contactType == "SYSTEM" && extensionNumber == 0) {
        group = true;
        groups.insert(0, this);
    }

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
    local[extensionNumber] = this;
}

QList<ContactItem *> ContactItem::findAuth(const QString& id)
{
    QList<ContactItem *> list;

    for(auto pos = 1; pos <= highest; ++pos) {
        auto item = local[pos];
        if(!item)
            continue;
        if(item->user() != id)
            continue;
        list << item;
    }

    return list;
}

void ContactItem::removeMember(ContactItem *item)
{
    if(contactLeader == item)
        contactLeader = nullptr;
    auto pos = contactMembers.indexOf(item);
    if(pos > -1)
        contactMembers.takeAt(pos);
}

void ContactItem::removeIndex(ContactItem *item)
{
    auto number = item->number();
    if(number < 1 || number > 999)
        return;
    auto pos = groups.indexOf(item);
    if(pos > -1)
        groups.takeAt(pos);
    pos = users.indexOf(item);
    if(pos > -1)
        users.takeAt(pos);
    local[number] = nullptr;
    index.remove(item->uid);

    for(auto pos = 0; pos <= highest; ++pos) {
        auto entry = local[pos];
        if(entry)
            entry->removeMember(item);
    }
    delete item;
}

void ContactItem::purge()
{
    foreach(auto item, index.values()) {
        // We can get a null key in the index if we do a lookup of a uid
        // that doesn't actually exist, such as from an un-attached message.
        if(!item)
            continue;
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

MemberModel::MemberModel(ContactItem *item) :
QAbstractListModel()
{
    membership = item->contactMembers;
    admin = item->admin;
    if(!admin)
        return;

    for(auto index = 0; index < 999; ++index) {
        item = local[index];
        if(!item)
            continue;
        if(item->isGroup())
            continue;
        if(membership.indexOf(item) > -1)
            continue;
        nonmembers << item;
    }
}

int MemberModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    if(admin)
        return membership.count() + nonmembers.count();
    else
        return membership.count();
}

QVariant MemberModel::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    ContactItem *item = nullptr;
    bool separator = false;
    QStringList list;

    if(row < 0)
        return QVariant();

    if(row < membership.count())
        item = membership[row];
    else {
        if(row == membership.count() && row > 0)
            separator = true;
        row -= membership.count();
        if(row < nonmembers.count())
            item = nonmembers[row];
    }
    if(!item)
        return QVariant();

    switch(role) {
    case DisplayNumber:
        return QString::number(item->number());
    case DisplayName:
        return item->display();
    case DisplayOnline:
        return item->isOnline();
    case DisplaySuspended:
        return item->isSuspended();
    case DisplaySeparator:
        return separator;
    case DisplayMembers:
        foreach(auto member, membership)
            list << QString::number(member->number());
        return list;
    case Qt::DisplayRole:
        return QString::number(item->number()) + " " + item->display();
    default:
        return QVariant();
    }
}

void MemberModel::remove(ContactItem *item)
{
    Q_ASSERT(item != nullptr);
    auto pos = membership.indexOf(item);
    if(pos > -1) {
        beginRemoveRows(QModelIndex(), pos, pos);
        membership.takeAt(pos);
        endRemoveRows();
        return;
    }
    pos = nonmembers.indexOf(item);
    if(pos > -1) {
        auto row = pos + membership.count();
        beginRemoveRows(QModelIndex(), row, row);
        nonmembers.takeAt(pos);
        endRemoveRows();
    }
}

void MemberModel::updateOnline(ContactItem *item)
{
    if(!item)
        return;
    auto pos = membership.indexOf(item);
    if(pos >= 0) {
        emit dataChanged(index(pos), index(pos));
        return;
    }
}

void LocalContacts::remove(int number)
{
    if(number > highest || number < 0)
        return;
    local[number] = nullptr;
    dataChanged(index(number), index(number));
}

int LocalContacts::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    if(highest < 0)
        return 0;

    return highest + 3;
}

QVariant LocalContacts::data(const QModelIndex& index, int role) const
{
    int row = index.row();

    if(row < 0 || row >= highest || role!= Qt::DisplayRole)
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

void LocalContacts::changeOnline(int number, bool status)
{
    if(number < 0 || number > 1000)
        return;

    auto item = local[number];
    if(!item)
        return;

    if(status == item->online)
        return;

    item->online = status;
    dataChanged(index(number), index(number));
    if(item->session && item != Phonebook::self())
        SessionModel::update(item->session);
}

ContactItem *LocalContacts::updateContact(const QJsonObject& json)
{
    auto storage = Storage::instance();
    if(!storage)
        return nullptr;

    auto status = json["s"].toString();
    auto sync = QDateTime::fromString(json["c"].toString(), Qt::ISODate);
    auto user = json["a"].toString();

    if(status == "remove" && removes > -1) {
        auto list = ContactItem::findAuth(user);
        if(list.count() < 1)
            return nullptr;

        foreach(auto item, list) {
            if(item == Phonebook::self()) {
                removes = -1;
                return nullptr;
            }
        }

        auto desktop = Desktop::instance();
        desktop->removeUser(user);
        return nullptr;
    }

    auto number = json["n"].toInt();
    if(number < 0 || number > 1000)
        return nullptr;

    auto group = json["g"].toString();
    auto type = json["t"].toString();
    auto display = json["d"].toString();
    auto uri = json["u"].toString();
    auto puburi = json["p"].toString();
    auto mailto = json["e"].toString();
    auto info = json["i"].toString();
    auto members = json["m"].toString();
    auto item = local[number];

    if(item && !status.isEmpty()) {
        if((type == "USER" || type == "DEVICE") && Desktop::isAdmin() && item != Phonebook::self() && item->user() != Phonebook::self()->user()) {
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

    if(item && (Desktop::isAdmin() || group != "none") && (number == 0 || type == "GROUP" || type == "PILOT")) {
        item->admin = Desktop::isAdmin();
        item->contactLeader = nullptr;
        if(group == "admin") {
            item->contactLeader = Phonebook::self();
            item->admin = true;
        }
        else {
            auto lead = group.toInt();
            if(lead > 1 && lead < 999) {
                auto leader = local[lead];
                if(leader && !leader->isGroup())
                    item->contactLeader = leader;
            }
        }
        ui.profileStack->setCurrentWidget(ui.memberPage);
        ui.memberPage->setEnabled(true);
        if(Desktop::isAdmin())
            ui.groupAdmin->setVisible(true);
        else
            ui.groupAdmin->setVisible(false);
    }

    if(!item) {
        storage->runQuery("INSERT INTO Contacts(extension, dialing, type, display, user, uri, mailto, puburi, sync, info) VALUES(?,?,?,?,?,?,?,?,?,?);", {
                              number, QString::number(number), type, display, user, uri, mailto, puburi, sync, info});
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
        item->contactType = type;
        item->contactCreated = sync;

        storage->runQuery("UPDATE Contacts SET display=?, user=?, type=?, uri=?, mailto=?, puburi=?, sync=?, info=? WHERE (extension=?);", {
                              display, user, type, uri, mailto, puburi, sync, info, number});

        if(type == "GROUP" || type == "PILOT" || number == 0)
            item->group = true;
        else
            item->group = false;

        item->contactMembers.clear();
        if(item->group && !members.isEmpty()) {
            auto list = members.split(",");
            foreach(auto member, list) {
                int mid = member.toInt();
                if(mid < 1 || mid > 999)
                    continue;
                auto contact = local[mid];
                if(contact  && !contact->hidden)
                    item->contactMembers << contact;
            }
        }

        if(item->hidden)
            item->hidden = false;
        else if(old == item->displayName)
            return item;

        if(item->session)
            SessionModel::update(item->session);
        else if(item == Phonebook::self())
            Desktop::instance()->setSelf(QString::fromUtf8(item->displayName));
    }

    dataChanged(index(number), index(number));
    return item;
}

bool MemberDelegate::eventFilter(QObject *list, QEvent *event)
{
    switch(event->type()) {
    case QEvent::MouseButtonRelease: {
        auto mpos = (static_cast<QMouseEvent *>(event))->pos();
        auto index = ui.memberList->indexAt(mpos);
        auto number = index.data(MemberModel::DisplayNumber).toInt();
        auto extension = index.data(MemberModel::DisplayNumber).toString();
        auto separator = index.data(MemberModel::DisplaySeparator).toBool();
        UString reason;

        if(!ui.memberList->isEnabled())
            break;

        if(number < 1 || number > 999 || local[number] == nullptr || local[number]->isGroup())
            break;

        auto item = local[number];
        auto height = ui.memberList->visualRect(index).height();
        if(separator) {
            mpos.ry() -= 8;
            height -= 8;
        }
        auto image = QRect(0, 0, height, height);
        mpos -= ui.memberList->visualRect(index).topLeft();
        if(!image.contains(mpos))
            break;

        auto members = index.data(MemberModel::DisplayMembers).toStringList();
        auto notify = members;
        auto phonebook = Phonebook::instance();
        int pos = members.indexOf(extension);
        if(pos > -1) {
            members.takeAt(pos);
            reason = "removing @" + UString::number(number) + " " + item->display() + " from group";
        }
        else {
            notify << extension;
            members << extension;
            reason = "adding @" + UString::number(number) + " " + item->display() + " to group";
        }
        qDebug() << "GROUP MEMBER" << members.join(",");
        phonebook->changeMembership(activeItem, members.join(","), notify.join(","), reason);
        ui.memberPage->setEnabled(false);
        break;
    }
    default:
        break;
    }
    return QStyledItemDelegate::eventFilter(list, event);
}


QSize MemberDelegate::sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    auto extension = index.data(MemberModel::DisplayNumber).toString();
    auto separator = index.data(MemberModel::DisplaySeparator).toBool();

    if(extension.isEmpty())
        return {0, 0};
    auto offset = 0;
    if(separator)
        offset += 8;
    return {style.rect.width(), cellHeight + offset};
}

void MemberDelegate::paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    auto extension = index.data(MemberModel::DisplayNumber).toString();
    auto display = index.data(MemberModel::DisplayName).toString();
    auto online = index.data(MemberModel::DisplayOnline).toBool();
    auto suspended = index.data(MemberModel::DisplaySuspended).toBool();
    auto pos = style.rect.bottomLeft();
    auto model = dynamic_cast<const MemberModel*>(index.model());
    auto width = style.rect.width() - 32;

    if(model->isAdmin()) {
        auto pen = painter->pen();
        auto render = painter->renderHints();
        QRect image = style.rect;
        if(model->isSeparator(index))
            image.setY(image.y() + 8);

        image.setTop(image.top() + 4);
        image.setLeft(image.left() + 4);
        image.setHeight(image.height() - 4);
        image.setWidth(image.height());
        painter->setPen(QColor("gray"));
        painter->setRenderHint(QPainter::Antialiasing);
        if(model->isMember(index))
            delIcon.paint(painter, image);
        else
            addIcon.paint(painter, image);
        painter->drawEllipse(image);
        pos.rx() += 20;
        width -= 20;
        painter->setPen(pen);
        painter->setRenderHints(render);
    }

    pos.ry() -= cellLift;
    auto pen = painter->pen();
    if(suspended)
        painter->setPen(QColor("red"));
    else if(online)
        painter->setPen(QColor("green"));
    painter->drawText(pos, extension);
    painter->setPen(pen);
    pos.rx() += 32;
    auto metrics = painter->fontMetrics();
    auto text = metrics.elidedText(display, Qt::ElideRight, width);
    painter->drawText(pos, text);
}

QSize LocalDelegate::sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    Q_UNUSED(style);

    auto row = index.row();
    auto rows = highest + 1;

    if(row > highest && Desktop::isAdmin())
        return {ui.contacts->width(), cellHeight - cellLift};

    if(row < 0 || row >= rows)
        return {0, 0};

    auto item = local[row];
    if(item == nullptr || item->display().isEmpty() || item->isHidden())
        return {0, 0};

    auto filter = item->filter();
    if(searching.length() > 0 && filter.length() > 0) {
        if(filter.indexOf(searching) < 0 && searching != item->authUserId)
            return {0, 0};
    }

    return {ui.contacts->width(), cellHeight};
}

void LocalDelegate::paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    if(style.rect.isEmpty())
        return;

    auto item = local[index.row()];
    auto pos = style.rect.bottomLeft();
    auto row = index.row();

    if(item && item == clickedItem && index.row()) {
        painter->fillRect(style.rect, QColor(CONST_CLICKCOLOR));
        clickedItem = nullptr;
    }

    if(row == highest + 1) {
        QRect image = style.rect;
        image.setTop(image.top() + 3);
        image.setWidth(image.height());
        addIcon.paint(painter, image);
        pos.rx() += 32;
        painter->drawText(pos, tr("Add..."));
    }
    else if(row == highest + 2) {
        QRect image = style.rect;
        image.setTop(image.top() + 3);
        image.setWidth(image.height());
        newIcon.paint(painter, image);
        pos.rx() += 32;
        painter->drawText(pos, tr("Add Group..."));
    }
    else if(item) {
        pos.ry() -= cellLift;  // off bottom
        auto pen = painter->pen();
        if(item->suspended)
            painter->setPen(QColor("red"));
        else if(item->online)
            painter->setPen(QColor("green"));
        painter->drawText(pos, item->textNumber);
        painter->setPen(pen);
        pos.rx() += 32;
        int width = style.rect.width() - 32;
        auto metrics = painter->fontMetrics();
        auto text = metrics.elidedText(item->textDisplay, Qt::ElideRight, width);
        painter->drawText(pos, text);
    }
}

Phonebook::Phonebook(Desktop *control, Sessions *sessions) :
QWidget(), desktop(control), localModel(nullptr), connector(nullptr), refreshRoster()
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;
    requestPending = false;
    cellHeight = QFontInfo(desktop->getBasicFont()).pixelSize() + 5;

    ui.setupUi(static_cast<QWidget *>(this));
    ui.contacts->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui.contact->setVisible(false);
    ui.logoButton->setVisible(false);
    ui.logoText->setVisible(false);

    addIcon = QIcon(":/icons/add.png");
    delIcon = QIcon(":/icons/del.png");
    newIcon = QIcon(":/icons/newgroup.png");

    connector = nullptr;

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

    connect(desktop, &Desktop::logout, this, [this] {
        activeItem = clickedItem = nullptr;
    });

    connect(ui.removeGroup, &QPushButton::pressed, this, [=] {
       if(!activeItem)
           return;
       control->openDelAuth(activeItem->user());
    });

    connect(ui.removeButton, &QPushButton::pressed, this, [=] {
        if(!activeItem)
            return;
        control->openDelAuth(activeItem->user());
    });

    connect(ui.contacts, &QListView::doubleClicked, this, [=](const QModelIndex& index) {
        auto row = index.row();
        if(row < 0 || row > highest)
            return;

        auto item = local[row];
        if(!item)
            return;

        QTimer::singleShot(CONST_CLICKTIME, this, [=] {
            sessions->activateContact(item);
        });
    });

    connect(desktop, &Desktop::changeListener, this, [this](Listener *listener) {
        Q_ASSERT(listener != nullptr);
        connect(listener, &Listener::changeStatus, this, &Phonebook::changeStatus);
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
    memberPainter = new MemberDelegate(this);
    ui.contacts->setItemDelegate(localPainter);
    ui.memberList->setItemDelegate(memberPainter);
    ui.memberList->viewport()->installEventFilter(memberPainter);
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

void Phonebook::changeMembership(ContactItem *item, const UString& members, const UString& notify, const UString& reason)
{
    if(!item || !connector)
        return;

    auto admin = item->groupAdmin();
    if(admin)
        connector->changeMemebership(item->uri(), item->topic().toUtf8(), members, UString::number(admin->number()), notify, reason);
    else
        connector->changeMemebership(item->uri(), item->topic().toUtf8(), members, "", notify, reason);
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
    clearGroup();
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
    clearGroup();
}

void Phonebook::changeProfile()
{
    if(!activeItem)
        return;

    if(activeItem->number() < 1 || activeItem->number() > 999) {
        desktop->errorMessage(tr("Cannot modify this contact"));
        ui.displayName->setText(activeItem->display());
        ui.emailAddress->setText(activeItem->mailTo());
        ui.since->setText(activeItem->sync().date().toString(Qt::DefaultLocaleShortDate));
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
    clearGroup();
}

void Phonebook::updateProfile(ContactItem *item)
{       
    if(!item || item != activeItem)
        return;

    auto type = item->type().toLower();
    ui.displayName->setText(item->display());
    ui.emailAddress->setText(item->mailTo());
    ui.since->setText(activeItem->sync().date().toString(Qt::DefaultLocaleShortDate));
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
    if(item == self() || item->isGroup()) {
        ui.behaviorGroup->setVisible(false);
        ui.behaviorGroup->setEnabled(false);
    }
    else {
        ui.behaviorGroup->setVisible(true);
        ui.behaviorGroup->setEnabled(true);
    }
}

void Phonebook::selectContact(const QModelIndex& index)
{
    ui.profileStack->setCurrentWidget(ui.blankPage);
    clearGroup();

    auto row = index.row();

    if(row == highest + 1) {
        Desktop::instance()->openAddUser();
        return;
    }

    if(row == highest + 2) {
        Desktop::instance()->openNewGroup();
        return;
    }

    if(row < 0 || row > highest)
        return;

    auto item = local[row];

    if(!item)
        return;

    if(item == self() || item->isGroup())
        ui.behaviorGroup->setVisible(false);
    else
        ui.behaviorGroup->setEnabled(true);

    if(connector) {
        ui.behaviorGroup->setEnabled(true);
        ui.forwardGroup->setEnabled(true);
    }
    else {
        ui.behaviorGroup->setEnabled(false);
        ui.forwardGroup->setEnabled(false);
    }

    activeItem = clickedItem = item;
    updateProfile(item);
    localModel->clickContact(row);
    if(connector && activeItem->number() > -1) {
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
            if(activeItem->isOnline())
                Toolbar::setStyle("color: green;");
        }
    });
}

ContactItem *Phonebook::oper()
{
    return local[0];
}

ContactItem *Phonebook::self()
{
    if(me < 1)
        return nullptr;

    return local[me];
}

ContactItem *Phonebook::find(int extension)
{
    if(extension < 0 || extension > 999)
        return nullptr;

    return local[extension];
}

void Phonebook::changeStatus(const QByteArray& status, int first, int last)
{
    unsigned pos = 0, mask = 1;
    auto count = first;
    auto *cp = reinterpret_cast<const unsigned char *>(status.constData());
    while(count < last) {
        auto online = (cp[pos] & mask) != 0;
        if(localModel)
            localModel->changeOnline(count, online);
        if(memberModel)
            memberModel->updateOnline(local[count]);

        if((mask <<= 1) > 128) {
            mask = 1;
            ++pos;
        }
        ++count;
    }
    auto item = activeItem;
    if(!desktop->isCurrent(this))
        item = nullptr;
    if(!item && Sessions::current())
        item = Sessions::current()->contactItem();
    if(!item)
        return;
    if(item->isOnline())
        Toolbar::setStyle("color: green;");
    else
        Toolbar::setStyle("color: black;");
}

void Phonebook::changeConnector(Connector *connected)
{
    connector = connected;
    requestPending = false;

    if(connector) {
        ui.behaviorGroup->setEnabled(true);
        ui.forwardGroup->setEnabled(true);
        requestPending = true;
        connect(connector, &Connector::changeProfile, this, [=](const QByteArray& json) {
            if(!connector)
                return;

            auto jdoc = QJsonDocument::fromJson(json);
            desktop->clearMessage();
            updateProfile(localModel->updateContact(jdoc.object()));
            updateGroup();
        }, Qt::QueuedConnection);

        connect(connector, &Connector::changeRoster, this, [=](const QByteArray& json) {
            if(!connector)
                return;

            desktop->clearRoster();
            refreshRoster.start(3600000);   // once an hour...

            auto jdoc = QJsonDocument::fromJson(json);
            auto list = jdoc.array();
            auto number = 0;

            if(activeItem)
                number = activeItem->number();

            removes = 0;
            foreach(auto profile, list) {
                updateProfile(localModel->updateContact(profile.toObject()));

                // cleaner way to get do self remove
                if(removes < 0) {
                    emit removeSelf();
                    return;
                }
            }

            // if active item deleted, reset...
            if(number > 1 && number <= highest && local[number] == nullptr) {
                activeItem = nullptr;
                enter();
            }
            updateGroup();

            if(removes || requestPending) {
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

        if(activeItem)
            connector->requestProfile(activeItem->uri());
    }
    else {
        ui.profileStack->setCurrentWidget(ui.blankPage);
        clearGroup();
        refreshRoster.stop();
        QByteArray status(((highest - 100) / 8) + 1, 0);
        changeStatus(status, 100, highest);
        ui.behaviorGroup->setEnabled(false);
        ui.forwardGroup->setEnabled(false);
    }
    if(localModel)
        localModel->changeLayout();
}

void Phonebook::updateGroup()
{
    if(ui.profileStack->currentWidget() != ui.memberPage || !activeItem || !activeItem->isGroup()) {
        clearGroup();
        return;
    }
    memberModel = new MemberModel(activeItem);
    ui.memberList->setModel(memberModel);
}

void Phonebook::clearGroup()
{
    if(memberModel) {
        ui.memberList->setModel(nullptr);
        delete memberModel;
        memberModel = nullptr;
    }
}

void Phonebook::changeStorage(Storage *storage)
{
    clearGroup();

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

void Phonebook::remove(ContactItem *item)
{
    Q_ASSERT(item != nullptr);
    auto number = item->number();
    if(number > 0) {
        if(memberModel)
            memberModel->remove(item);
    }
    if(localModel)
        localModel->remove(number);
    ContactItem::removeIndex(item);
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

