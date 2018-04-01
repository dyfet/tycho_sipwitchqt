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
#include "../Connect/storage.hpp"

#include <QPainter>
#include <QString>
#include <QFont>
#include <QTextLayout>
#include <QTextOption>
#include <QRegularExpression>

static QFont textFont;                      // default message text font
static QFont timeFont;
static QFont userFont;
static QFont lineFont;
static QFont monoFont;                      // default mono font
static QFont boldFont;
static int textHeight, monoHeight;
static QTextOption textOptions;             // default text layout options
static QTextOption textCentered;            // centered layouts...
static QTextOption textRight;
static QList<QString> dayOfWeek;
static QList<QString> monthOfYear;
static QString dayToday, dayYesterday;
static QColor userColor(120, 0, 120);
static QColor selfColor(16, 120, 64);
static QColor groupColor(0, 64, 120);
static QColor searchColor(240, 240, 0);
static QColor urlColor(0, 0, 240);
static QColor mapColor(0, 0, 244);
static QColor nbrColor(240, 120, 0);
static QRegularExpression findGroup(R"([\#]\d\d\d)");
static QRegularExpression findUser(R"([\@]\d\d\d)");
static QRegularExpression findHttp(R"((http(s)?:\/\/.)?(www\.)?[-a-zA-Z0-9@:%._\+~#=]{2,256}\.[a-z]{2,6}\b([-a-zA-Z0-9@:%_\+.~#?&//=]*))");
static QRegularExpression findMap(R"([-+]?([1-8]?\d(\.\d+)?|90(\.0+)?),\s*[-+]?(180(\.0+)?|((1[0-7]\d)|([1-9]?\d))(\.\d+)?))");
static QRegularExpression findDialing(R"(\+(?:[0-9] ?){6,14}[0-9])");
static QList<QPair<QRegularExpression, QColor>> findMatches = {{findUser, userColor}, {findGroup, groupColor}, {findHttp, urlColor}, {findMap, mapColor}, {findDialing, nbrColor}};
static int dynamicLine;

MessageItem::MessageItem(SessionItem *sid, ContactItem *from, ContactItem *to, const UString& text, const QDateTime& timestamp, int sequence, const UString& subject) :
session(sid)
{
    if(timestamp.isValid())
        dateTime = timestamp;
    else
        dateTime = QDateTime::currentDateTime();

    dayNumber = Util::currentDay(dateTime);
    dateSequence = sequence;
    msgType = TEXT_MESSAGE;
    msgSubject = subject;
    msgBody = text;
    msgFrom = from;
    msgTo = to;

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
    inbox = false;

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
    dateSequence = record.value("seqid").toInt();
    msgSubject = record.value("subject").toString().toUtf8();
    dayNumber = Util::currentDay(dateTime);
    saved = true;

    auto type = record.value("msgtype").toString();
    if(type == "TEXT") {
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
    textFormats = 0;
    textUnderline = -1;
    userUnderline = false;

    foreach(auto search, findMatches) {
        auto match = search.first.match(msgBody);
        auto pos = 0;
        for(;;) {
            auto start = match.capturedStart(pos);
            if(start < 0)
                break;
            QTextLayout::FormatRange range;
            range.start = start;
            range.length = match.capturedLength(pos++);
            range.format.setForeground(search.second);
            formats << range;
            ++textFormats;
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

    if(!userHint || pos.y() > lineHint || pos.y() < (lineHint - userHeight))
        userUnderline = false;
    else
        userUnderline = true;

    if(textUnderline > -1)
        formats[textUnderline].format.setFontUnderline(false);

    textUnderline = -1;
    if(pos.x() > 2 && pos.x() < lastHint.width() - dynamicLine && pos.y() > lineHint) {
        auto bottom = lineHint;
        auto count = 0;
        foreach(auto line, textLines) {
            bottom += line.height();
            if(pos.y() < bottom)
                break;
            bottom += 4;
            ++count;
        }
        if(count < textLines.count()) {
            auto textPos = textLines[count].xToCursor(pos.x(), QTextLine::CursorOnCharacter);
            auto entry = 0;
            foreach(auto format, formats) {
                if(textPos >= format.start && textPos < format.start + format.length) {
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

    auto mid = storage->insert("INSERT INTO Messages(msgfrom, msgto, sid, seqid, posted, msgtype, msgtext) "
                      "VALUES(?,?,?,?,?,?,?);", {
                          msgFrom->uid,
                          msgTo->uid,
                          session->contact->uid,
                          dateSequence,
                          dateTime,
                          "TEXT",
                          msgBody
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

    if(row == 0 || session->filtered[row-1]->msgFrom != msgFrom)
        userHint = true;
    else
        userHint = dateHint;

    if(!userHint && row > 0 && session->filtered[row-1]->dateTime.secsTo(dateTime) >= 150)
        timeHint = true;
    else
        timeHint = false;

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
        height += dateHeight;
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
        textDisplay.setTextFormat(Qt::RichText);
        userHeight = QFontInfo(userFont).pixelSize() + 4;
        if(!dateHint)
            userHeight += 4;
        height += userHeight;
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
    height += textHeight + 4;

    if(height && !scrollHint)
        lastHint = QSize(width, height);
    else
        lastHint = QSize(0, 0);

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

    listView = (static_cast<QListView*>(parent));
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
    if(!item->saved)
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
    auto increment = static_cast<int>(userFont.pointSize() * 6);

    if(!session || row < 0 || row > session->filtered.count())
        return;

    auto item = session->filtered[row];
    if(!item)
        return;

    if(item->leadHeight > 0.)
        position.ry() += item->leadHeight;

    if(item->dateHint) {
        auto linepos = position;
        auto pen = painter->pen();
        auto width = (static_cast<int>(style.rect.width() - item->textDateline.textWidth()) / 2 - 8);
        linepos.ry() += item->textDateline.size().height() / 2;
        painter->setPen(QColor("#f0f0f0"));
        painter->drawLine(linepos.x(), linepos.y(), linepos.x() + width, linepos.y());
        linepos.rx() += (style.rect.width() - width);
        painter->drawLine(linepos.x(), linepos.y(), linepos.x() + width, linepos.y());
        painter->setPen(pen);
        auto datepos = position;
        datepos.rx() += ((style.rect.width() - item->textDateline.textWidth()) / 2);
        painter->setFont(boldFont);
        painter->drawStaticText(datepos, item->textDateline);
        position.ry() += item->dateHeight;
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
        position.ry() += item->userHeight;
    }
    else if(item->timeHint) {
        position.setX(style.rect.right() - dynamicLine);
        painter->setFont(timeFont);
        painter->drawStaticText(position, item->textTimestamp);
    }

//    QLine line;
    painter->setFont(textFont);
    position.setX(style.rect.left());
    item->textLayout.draw(painter, position);
    painter->setFont(style.font);
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

        auto mpos = (static_cast<QMouseEvent *>(event))->pos();
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
        auto model = static_cast<MessageModel*>(listView->model());
        if(!model)
            break;
        auto mpos = (static_cast<QMouseEvent *>(event))->pos();
        auto index = listView->indexAt(mpos);
        model->hover(index, mpos - listView->visualRect(index).topLeft());
        break;
    }
    default:
        break;
    }
    return QStyledItemDelegate::eventFilter(list, event);
}
