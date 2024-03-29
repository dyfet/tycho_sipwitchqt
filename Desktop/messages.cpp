/*
 * Copyright (C) 2017-2018 Tycho Softworks.
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
#include "../Connect/storage.hpp"

#include <QPainter>
#include <QString>
#include <QFont>
#include <QTextLayout>
#include <QTextOption>
#include <QRegularExpression>

namespace {
enum class TextFormat {
    bold, italic, mono
};

QFont textFont;                      // default message text font
QFont textMono;
QFont timeFont;
QFont userFont;
QFont lineFont;
QFont monoFont;                      // default mono font
QFont boldFont;
int textHeight, monoHeight;
QTextOption textOptions;             // default text layout options
QTextOption textCentered;            // centered layouts...
QTextOption textRight;
QList<QString> dayOfWeek;
QList<QString> monthOfYear;
QString dayToday, dayYesterday;
QColor userColor(120, 0, 120);
QColor selfColor(16, 0, 192);
QColor groupColor(0, 64, 120);
QColor searchColor(240, 240, 0);
QColor urlColor(0, 0, 240);
QColor mapColor(0, 0, 244);
QColor nbrColor(240, 120, 0);
QColor tinted(255, 255, 223);
QRegularExpression findBold(R"(\*\*(\w*?)\*\*)");
QRegularExpression findItalic(R"(_(\w*?)_)");
QRegularExpression findMono(R"(`(\w*?)`)");
QRegularExpression findGroup(R"([\#]\d\d\d)");
QRegularExpression findUser(R"([\@]\d\d\d)");
QRegularExpression findHttp(R"((http(s)?:\/\/.)?(www\.)?[-a-zA-Z0-9@:%._\+~#=]{2,256}\.[a-z]{2,6}\b([-a-zA-Z0-9@:%_\+.~#?&//=]*))");
QRegularExpression findMap(R"([-+]?([1-8]?\d(\.\d+)?|90(\.0+)?),\s*[-+]?(180(\.0+)?|((1[0-7]\d)|([1-9]?\d))(\.\d+)?))");
QRegularExpression findDialing(R"(\+(?:[0-9] ?){6,14}[0-9])");
QList<QPair<QRegularExpression, QColor>> findMatches = {{findUser, userColor}, {findGroup, groupColor}, {findHttp, urlColor}, {findMap, mapColor}, {findDialing, nbrColor}};
QList<QPair<QRegularExpression, TextFormat>> fontMatches = {{findBold, TextFormat::bold}, {findItalic, TextFormat::italic}, {findMono, TextFormat::mono}};
int dynamicLine;
} // namespace

MessageItem::MessageItem(SessionItem *sid, ContactItem *from, ContactItem *to, const UString& text, const QDateTime& timestamp, int sequence, const UString& subject, const QString& type) :
session(sid)
{
    if(timestamp.isValid())
        dateTime = timestamp;
    else
        dateTime = QDateTime::currentDateTime();

    expires = dateTime.addSecs(Desktop::instance()->expiration());
    dayNumber = Util::currentDay(dateTime);
    dateSequence = sequence;
    msgType = TEXT_MESSAGE;
    msgSubject = subject;
    msgBody = text;
    msgFrom = from;
    msgTo = to;
    hidden = false;

    if(type == "text/admin")
        msgType = ADMIN_MESSAGE;

    if(msgFrom->isGroup()) {
        itemColor = groupColor;
        userString = "#" + msgFrom->textNumber;
    }
    else {
        itemColor = userColor;
        userString = "@" + msgFrom->textNumber;
    }

    textString = text;
    if(msgFrom == Phonebook::self()) {
        itemColor = selfColor;
        inbox = false;
    }
    else
        inbox = true;

    if(sid->mostRecent < dateTime) {
        sid->mostRecent = dateTime;
        sid->lastSequence = sequence;
    }
    else if(sid->mostRecent == dateTime && sid->lastSequence < sequence)
        sid->lastSequence = sequence;

    findFormats();
    save();
}

MessageItem::MessageItem(SessionItem *sid, const QString& text, const QDateTime &timestamp, int sequence) :
session(sid)
{
    if(timestamp.isValid())
        dateTime = timestamp;
    else
        dateTime = QDateTime::currentDateTime();

    expires = dateTime.addSecs(Desktop::instance()->expiration());
    dayNumber = Util::currentDay(dateTime);
    dateSequence = sequence;
    msgType = TEXT_MESSAGE;
    msgSubject = session->topic();
    msgBody = text.toUtf8();
    msgFrom = Phonebook::self();
    msgTo = session->contactItem();
    itemColor = selfColor;
    userString = "@" + msgFrom->textNumber;
    textString = text;
    inbox = hidden = false;

    findFormats();
    save();
}

MessageItem::MessageItem(const QSqlRecord& record) :
session(nullptr), saved(false)
{
    auto sid = record.value("sid").toInt();
    auto from = record.value("msgfrom").toInt();
    auto to = record.value("msgto").toInt();

    auto contact = ContactItem::findContact(sid);
    if(contact)
        session = contact->session;

    if(!session) {
        qDebug() << "*** FAILED LOAD";
        return;
    }

    msgFrom = ContactItem::findContact(from);
    msgTo = ContactItem::findContact(to);
    if(!msgFrom || !msgTo) {
        qDebug() << "*** UNLINKED MESSAGE";
        session = nullptr;
        return;
    }

    msgBody = record.value("msgtext").toByteArray();
    dateTime = record.value("posted").toDateTime();
    expires = record.value("expires").toDateTime();
    dateSequence = record.value("seqid").toInt();
    msgSubject = record.value("subject").toString().toUtf8();
    dayNumber = Util::currentDay(dateTime);
    saved = true;
    hidden = false;

    auto type = record.value("msgtype").toString();
    if(type == "ADMIN") {
        msgType = ADMIN_MESSAGE;
        textString = QString::fromUtf8(msgBody);
    }
    else {
        msgType = TEXT_MESSAGE;
        textString = QString::fromUtf8(msgBody);
    }

    if(msgFrom->isGroup()) {
        itemColor = groupColor;
        userString = "#" + msgFrom->textNumber;
    }
    else {
        itemColor = userColor;
        userString = "@" + msgFrom->textNumber;
    }

    if(msgFrom == Phonebook::self()) {
        inbox = false;
        itemColor = selfColor;
    }
    else
        inbox = true;

    findFormats();
}

void MessageItem::findFormats()
{
    textFormats = textFonts = 0;
    textUnderline = -1;
    userUnderline = false;

    foreach(auto search, fontMatches) {
        auto matches = search.first.globalMatch(msgBody);
        while(matches.hasNext()) {
            auto match = matches.next();
            auto start = match.capturedStart(0);
            if(start < 0)
                break;
            QTextLayout::FormatRange range;
            range.start = start;
            range.length = match.capturedLength(0);
            switch(search.second) {
            case TextFormat::bold:
                range.format.setFontWeight(75);
                break;
            case TextFormat::italic:
                range.format.setFontItalic(true);
                break;
            case TextFormat::mono:
                range.format.setFont(textMono);
                break;
            default:
                break;
            }
            formats << range;
            ++textFormats;
            ++textFonts;
        }
    }

    foreach(auto search, findMatches) {
        auto matches = search.first.globalMatch(msgBody);
        while(matches.hasNext()) {
            auto match = matches.next();
            auto pos = 0;
            for(;;) {
                auto start = match.capturedStart(pos);
                if(start < 0)
                    break;
                QTextLayout::FormatRange range;
                range.start = start;
                range.length = match.capturedLength(pos++);
                range.format.setForeground(search.second);
                range.format.setBackground(tinted);
                formats << range;
                ++textFormats;
            }
        }
    }
}

QPair<QString, enum ClickedItem> MessageItem::textClicked()
{
    if(userUnderline)
        return {userString.left(4), EXTENSION_CLICKED};
    else if(textUnderline > -1) {
        auto type = TEXT_CLICKED;
        auto color = formats[textUnderline].format.foreground();
        if(color == urlColor)
            type = URL_CLICKED;
        else if(color == nbrColor)
            type = NUMBER_CLICKED;
        else if(color == mapColor)
            type = MAP_CLICKED;
        else if(color == selfColor || color == groupColor || color == userColor)
            type = EXTENSION_CLICKED;
        return {textString.mid(formats[textUnderline].start, formats[textUnderline].length), type};
    }
    return {QString(), NOTHING_CLICKED};
}

void MessageItem::clearHover()
{
    if(textUnderline > -1) {
        formats[textUnderline].format.setFontUnderline(false);
        textUnderline = -1;
    }
    userUnderline = false;
}

bool MessageItem::hover(const QPoint& pos)
{
    auto priorUnderline = textUnderline;
    auto priorUser = userUnderline;

    userUnderline = !(!userHint || pos.y() > lineHint || pos.y() < (lineHint - userHeight));
    if(textUnderline > -1)
        formats[textUnderline].format.setFontUnderline(false);

    textUnderline = -1;
    if(pos.x() > 2 && pos.x() < lastHint.width() - dynamicLine && pos.y() > lineHint) {
        auto bottom = lineHint;
        auto count = 0;
        foreach(auto line, textLines) {
            bottom += static_cast<int>(line.height());
            if(pos.y() < bottom)
                break;
            bottom += 4;
            ++count;
        }
        if(count < textLines.count()) {
            auto textPos = textLines[count].xToCursor(pos.x(), QTextLine::CursorOnCharacter);
            auto entry = 0;
            foreach(auto format, formats) {
                if(textPos >= format.start && textPos < format.start + format.length && entry >= textFonts) {
                    textUnderline = entry;
                    break;
                }
                if(++entry >= textFormats)
                    break;
            }
        }
    }

    if(textUnderline > -1)
        formats[textUnderline].format.setFontUnderline(true);
    return (textUnderline != priorUnderline) || (userUnderline != priorUser);
}

void MessageItem::save()
{
    saved = true;
    if(!session->saved)
        return;

    auto storage = Storage::instance();
    Q_ASSERT(storage != nullptr);

    QString type = "TEXT";
    switch(msgType) {
    case ADMIN_MESSAGE:
        type = "ADMIN";
        break;
    default:
        break;
    }

    auto mid = storage->insert("INSERT INTO Messages(msgfrom, msgto, sid, seqid, posted, msgtype, msgtext, subject, expires) "
                      "VALUES(?,?,?,?,?,?,?,?,?);", {
                          msgFrom->uid,
                          msgTo->uid,
                          session->contact->uid,
                          dateSequence,
                          dateTime,
                          type,
                          msgBody,
                          QString::fromUtf8(msgSubject),
                          expires,
    });
    if(!mid.isValid())
        saved = false;
}

void MessageItem::addSearch(int pos, int len)
{
    QTextLayout::FormatRange range;
    range.start = pos;
    range.length = len;
    range.format.setBackground(searchColor);
    formats << range;
}

QSize MessageItem::layout(const QStyleOptionViewItem& style, int row, bool scrollHint)
{
    if(row == 0 || session->filtered[row - 1]->dayNumber != dayNumber)
        dateHint = true;
    else
        dateHint = scrollHint;

    if(msgType == ADMIN_MESSAGE)
        userHint = false;
    else if(row == 0 || session->filtered[row-1]->msgFrom != msgFrom)
        userHint = true;
    else
        userHint = dateHint;

    timeHint = !userHint && row > 0 && session->filtered[row - 1]->dateTime.secsTo(dateTime) >= 150;
    int height = 0;
    int width = style.rect.width();

    dateHeight = userHeight = textHeight = leadHeight = 0;
    textLayout.clearLayout();
    textLines.clear();
    dynamicLine = (textFont.pointSize() - 3) * 8;
    if (dynamicLine < 42)
        dynamicLine = 50;
    else if (dynamicLine < 83)
        dynamicLine -= 15 ;
    else if (dynamicLine > 83 )
        dynamicLine -= 20 ;
    else if (dynamicLine > 100)
        dynamicLine -= 35 ;
    if(dateHint) {
        QString dateString = dayToday;
        auto today = Util::currentDay();
        if(today != dayNumber) {
            auto date = dateTime.date();
            if(dayNumber + 1 == today)
                dateString = dayYesterday;
            else if(dayNumber + 4 >= today)
                dateString = dayOfWeek[date.dayOfWeek() - 1];
            else
                dateString = dayOfWeek[date.dayOfWeek() - 1] + "   " + monthOfYear[date.month() - 1] + " " + QString::number(date.day());
        }

        textDateline.setTextOption(textCentered);
        textDateline.setTextWidth(dynamicLine);
        textDateline.setText(dateString);
        textDateline.prepare(QTransform(), boldFont);
        dateHeight = QFontInfo(boldFont).pixelSize() + 4;
        if(row > 0) {
            leadHeight = 4;
            dateHeight += 4;
            height += 4;
        }
        height += static_cast<int>(dateHeight);
    }

    if(userHint) {
        QString status = "@";
        if(msgFrom->isGroup())
            status = "#";

        textTimestamp.setTextOption(textRight);
        textTimestamp.setTextWidth(dynamicLine);
        textTimestamp.setText(dateTime.time().toString(Qt::DefaultLocaleShortDate).toLower().replace(QString(" "), QString()));
        textStatus.setText(status + msgFrom->textNumber);
        textDisplay.setText(msgFrom->textDisplay);
        textDisplay.setTextWidth(width - dynamicLine - 24);
        textDisplay.setTextFormat(Qt::RichText);
        userHeight = QFontInfo(userFont).pixelSize() + 4;
        if(!dateHint)
            userHeight += 4;
        height += static_cast<int>(userHeight);
    }
    else if(timeHint)
    {
        textTimestamp.setTextOption(textRight);
        textTimestamp.setTextWidth(dynamicLine);
        textTimestamp.setText(dateTime.time().toString(Qt::DefaultLocaleShortDate).toLower().replace(QString(" "), QString()));
    }

    textLayout.clearFormats();
    textLayout.setFormats(formats);

    textLayout.setFont(textFont);
    textLayout.setText(textString);
    textLayout.setCacheEnabled(true);
    textLayout.setTextOption(textOptions);
    if(msgType == ADMIN_MESSAGE)
        textLayout.setFont(monoFont);
    else
        textLayout.setFont(textFont);
    textLayout.beginLayout();

    lineHint = height;
    textHeight = 0;
    textLines.clear();

    for(;;) {
        auto line = textLayout.createLine();
        if(!line.isValid())
            break;

        line.setLeadingIncluded(false);
        line.setLineWidth(width - dynamicLine);
        line.setPosition(QPointF(0, textHeight));
        textHeight += line.height();
        textLines << line;
    }
    height += static_cast<int>(textHeight) + 4;
    lastHint = height && !scrollHint ? QSize(width, height) : QSize(0, 0);

/*    if(msgType == ADMIN_MESSAGE) Maybe earlier???
        height += 4;
*/
    return lastHint;
}

int MessageModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);

    auto count = session->filtered.count();
    if(count < 0)
        count = 0;
    return count;
}

QVariant MessageModel::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    int rows = rowCount(index);

    if(row < 0 || row >= rows || role!= Qt::DisplayRole)
        return QVariant();

    auto item = session->filtered[row];
    if(item->msgBody.isEmpty())
        return QVariant();

    return item->msgBody;
}

void MessageModel::remove(ContactItem *item)
{
    int row = 0;
    // find any filtered messages attached to a deleted user
    QList<int> removes;
    foreach(auto msg, session->filtered) {
        if(msg->msgFrom == item || msg->msgTo == item) {
            msg->saved = false;
            removes.prepend(row);   // reverse order...
        }
        ++row;
    }

    foreach(row, removes) {
        beginRemoveRows(QModelIndex(), row, row);
        session->filtered.takeAt(row);
        endRemoveRows();
    }

    // get any that are outside the filter...
    removes.clear();
    foreach(auto msg, session->messages) {
        if(msg->msgFrom == item || msg->msgTo == item) {
            msg->saved = false;
            removes.prepend(row);
        }
    }
    foreach(row, removes)
        session->messages.takeAt(row);
}

int MessageModel::checkExpiration()
{
    auto now = QDateTime::currentDateTime();
    int row = 0, changed = 0, result = 0;
    QDateTime latest;

    foreach(auto msg, session->filtered) {
        auto hide = msg->expires < now;
        if(!hide && (!latest.isValid() || latest > msg->expires))
            latest = msg->expires;

        std::swap(hide, msg->hidden);
        if(hide != msg->hidden) {
            ++changed;
            dataChanged(index(row), index(row));
        }
        ++row;
    }
    foreach(auto msg, session->messages) {
        if(msg->hidden)
            continue;
        auto hide = msg->expires < now;
        if(!hide && (!latest.isValid() || latest > msg->expires))
            latest = msg->expires;
        if(hide)
            msg->hidden = hide;
    }
    if(latest.isValid()) {
        auto diff = latest.toMSecsSinceEpoch() - now.toMSecsSinceEpoch();
        if(diff < 1000l * 3600l * 24l * 7l) // cannot be longer than week
            result = static_cast<int>(diff);
    }
    qDebug() << "Changed visibility for" << changed << "of" << row << "latest" << result / 1000l << latest;
    return result;
}

void MessageModel::fastUpdate(int count)
{
    if(count > 0) {
        beginInsertRows(QModelIndex(), 0, count - 1);
        endInsertRows();
    }
}

void MessageModel::changeLayout()
{
    layoutAboutToBeChanged();
    layoutChanged();
}

bool MessageModel::add(MessageItem *item)
{
    int rowFiltered = session->filtered.count();
    int rowMessages = session->messages.count();
    bool fast = false;
    bool filtered = false;

    QDateTime firstDateTime;
    firstDateTime.setMSecsSinceEpoch(0);

    //TODO: filtered = true if filtered out by topic

    if(!filtered) {
        // reverse order optimization, will be used in future for lazy loader...
        if(rowFiltered > 0)
            firstDateTime = session->filtered[0]->dateTime;

        if(item->dateTime < firstDateTime || !rowFiltered) {
            fast = true;
            rowFiltered = 0;
        }
        else while(rowFiltered > 0) {
            auto last = session->filtered[rowFiltered - 1];
            if(last->dateTime < item->dateTime)
                break;
            if(last->dateTime == item->dateTime && last->dateSequence < item->dateSequence)
                break;
            --rowFiltered;
        }
    }

    if(rowMessages > 0)
        firstDateTime = session->messages[0]->dateTime;

    if(item->dateTime < firstDateTime)
        rowMessages = 0;
    else while(rowMessages > 0) {
        auto last = session->messages[rowMessages - 1];
        if(last->dateTime < item->dateTime)
            break;
        if(last->dateTime == item->dateTime && last->dateSequence < item->dateSequence)
            break;
        --rowMessages;
    }

    if(!fast)
        beginInsertRows(QModelIndex(), rowFiltered, rowFiltered);

    session->messages.insert(rowMessages, item);
    if(item->type() == MessageItem::ADMIN_MESSAGE && !item->subject().isEmpty()) {
        session->setTopic(item->subject(), item->posted(), item->sequence());
    }
    if(!filtered)
        session->filtered.insert(rowFiltered, item);
    if(!fast && rowFiltered < session->filtered.count() - 1)
        session->filtered[rowFiltered + 1]->lastHint = QSize(0, 0);
    if(!fast)
        endInsertRows();
    return fast;
}

QModelIndex MessageModel::last()
{
    return index(session->filtered.count() - 1);
}

bool MessageModel::hover(const QModelIndex& index, const QPoint& pos)
{
    auto row = lastHover.row();
    lastHover = index;
    if(row != index.row() && session && row >= 0 && row < session->filtered.count()) {
        session->filtered[row]->clearHover();
        emit dataChanged(index, index);
    }

    row = index.row();
    if(!session || row < 0 || row >= session->filtered.count())
        return false;

    auto item = session->filtered[row];
    if(item->hover(pos))
        emit dataChanged(index, index);
    return false;
}

MessageDelegate::MessageDelegate(QWidget *parent) :
QStyledItemDelegate(parent)
{
    auto desktop = Desktop::instance();

    listView = (dynamic_cast<QListView*>(parent));
    listView->viewport()->installEventFilter(this);

    textFont = userFont = lineFont = timeFont = boldFont = desktop->getCurrentFont();
    userFont.setPointSize(userFont.pointSize() - 1);
    lineFont.setPointSize(userFont.pointSize());
    lineFont.setUnderline(true);
    textFont.setPointSize(textFont.pointSize() + 1);
//    timeFont.setWeight(10);
    timeFont.setPointSize(textFont.pointSize() - 3);
    if(timeFont.pointSize() <7)
        timeFont.setPointSize(6);
    boldFont.setBold(true);

    monoFont = QFont("monospace");
    textMono = monoFont;
    textMono.setPointSize(textFont.pointSize() - 1);
    if(!QFontInfo(monoFont).fixedPitch()) {
        monoFont.setStyleHint(QFont::Monospace);
        if(!QFontInfo(monoFont).fixedPitch()) {
            monoFont.setStyleHint(QFont::TypeWriter);
            if (!QFontInfo(monoFont).fixedPitch())
                monoFont.setFamily("courier");
        }
    }
    monoFont.setPointSize(textFont.pointSize() - 1);

    textRight.setAlignment(Qt::AlignRight);
    textCentered.setAlignment(Qt::AlignCenter);
    textOptions.setAlignment(Qt::AlignLeft);
    textOptions.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    dayOfWeek = {
        tr("Monday"),
        tr("Tuesday"),
        tr("Wednesday"),
        tr("Thursday"),
        tr("Friday"),
        tr("Saturday"),
        tr("Sunday"),
    };

    monthOfYear = {
        tr("  January"),
        tr("  Febuary"),
        tr("    March"),
        tr("    April"),
        tr("      May"),
        tr("     June"),
        tr("     July"),
        tr("   August"),
        tr("September"),
        tr("  October"),
        tr(" November"),
        tr(" December"),
    };

    dayToday = tr("Today");
    dayYesterday = tr("Yesterday");

    textHeight = QFontInfo(textFont).pointSize();
    monoHeight = QFontInfo(monoFont).pointSize();
}

MessageDelegate::~MessageDelegate()
{
    listView->removeEventFilter(this);
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    auto row = index.row();
    auto session = Sessions::active();

    if(!session || row < 0 || row > session->filtered.count())
        return {0, 0};

    auto item = session->filtered[row];

    // kills dups and invalids
    if(!item->saved || item->hidden)
        return {0, 0};

//    if(item->lastHint.width() == style.rect.width())
//        return item->lastHint;

    return item->layout(style, row);
}

void MessageDelegate::paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    auto row = index.row();
    auto session = Sessions::active();
    auto position = style.rect.topLeft();
    auto increment = userFont.pointSize() * 6;

    if(!session || row < 0 || row > session->filtered.count())
        return;

    auto item = session->filtered[row];
    if(!item || !item->saved || item->hidden)
        return;

    if(item->leadHeight > 0.)
        position.ry() += static_cast<int>(item->leadHeight);

    if(item->dateHint) {
        auto linepos = position;
        auto pen = painter->pen();
        auto width = (static_cast<int>(style.rect.width() - item->textDateline.textWidth()) / 2 - 8);
        linepos.ry() += static_cast<int>(item->textDateline.size().height()) / 2;
        painter->setPen(QColor("#f0f0f0"));
        painter->drawLine(linepos.x(), linepos.y(), linepos.x() + width, linepos.y());
        linepos.rx() += (style.rect.width() - width);
        painter->drawLine(linepos.x(), linepos.y(), linepos.x() + width, linepos.y());
        painter->setPen(pen);
        auto datepos = position;
        datepos.rx() += static_cast<int>((style.rect.width() - item->textDateline.textWidth()) / 2);
        painter->setFont(boldFont);
        painter->drawStaticText(datepos, item->textDateline);
        position.ry() += static_cast<int>(item->dateHeight);
    }

    if(item->userHint) {
        auto userpos = position;
        auto pen = painter->pen();
        if(!item->dateHint)
            userpos.ry() += 4;
        if(item->userUnderline)
            painter->setFont(lineFont);
        else
            painter->setFont(userFont);
        painter->setPen(item->itemColor);
        painter->drawStaticText(userpos, item->textStatus);
        painter->setFont(userFont);
        userpos.rx() += increment;
        painter->drawStaticText(userpos, item->textDisplay);
        userpos.setX(style.rect.right() - dynamicLine);
        painter->setFont(timeFont);
        painter->setPen(pen);
        painter->drawStaticText(userpos, item->textTimestamp);
        position.ry() += static_cast<int>(item->userHeight);
    }
    else if(item->timeHint) {
        position.setX(style.rect.right() - dynamicLine);
        painter->setFont(timeFont);
        painter->drawStaticText(position, item->textTimestamp);
    }

//    QLine line;
    auto pen = painter->pen();
    painter->setFont(textFont);
    position.setX(style.rect.left());
    if(item->msgType == MessageItem::ADMIN_MESSAGE) {
        position.ry() += 2;
        painter->setPen(QColor("gray"));
    }
    item->textLayout.draw(painter, position);
    painter->setFont(style.font);
    painter->setPen(pen);
//    painter->drawLine();

    // TODO: Force paint a top line in view if no headers visible...
    /*
    if(style.rect.y() < 1 && row < session->filtered.count() - 1 && !session->filtered[row + 1]->dateHint) {
        qDebug() << "TOP LINE HINT REQUIRED";
    }
    else if(style.rect.y() < 1)
        qDebug() << "NO TOP";
    */
}

bool MessageDelegate::eventFilter(QObject *list, QEvent *event)
{
    switch(event->type()) {
    case QEvent::MouseButtonRelease: {
        auto session = Sessions::active();
        if(!session)
            break;

        auto mpos = (dynamic_cast<QMouseEvent *>(event))->pos();
        auto index = listView->indexAt(mpos);
        auto row = index.row();
        if(row < 0 || row >= session->filtered.count())
            break;

        auto clicked = session->filtered[row]->textClicked();
        if(clicked.first.isEmpty())
            break;

        Sessions::instance()->clickedText(clicked.first, clicked.second);
        break;
    }
    case QEvent::MouseMove: {
        auto model = dynamic_cast<MessageModel*>(listView->model());
        if(!model)
            break;
        auto mpos = (dynamic_cast<QMouseEvent *>(event))->pos();
        auto index = listView->indexAt(mpos);
        model->hover(index, mpos - listView->visualRect(index).topLeft());
        break;
    }
    default:
        break;
    }
    return QStyledItemDelegate::eventFilter(list, event);
}
