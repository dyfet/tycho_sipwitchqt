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

#include "phonebook.hpp"
#include "sessions.hpp"
#include "../Connect/storage.hpp"

#include <QPainter>
#include <QString>
#include <QFont>
#include <QGuiApplication>
#include <QTextLayout>
#include <QTextOption>

static int currentSequence = 0;
static QFont textFont;                      // default message text font
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
static QColor groupColor(0, 96, 120);

MessageItem::MessageItem(SessionItem *sid, const QString& text, bool save) :
session(sid)
{
    dateTime = QDateTime::currentDateTime();
    dayNumber = Util::currentDay(dateTime);
    dateSequence = ++currentSequence;
    msgType = TEXT_MESSAGE;
    msgSubject = session->topic();
    msgBody = text.toUtf8();
    msgFrom = Phonebook::self();
    msgTo = session->contactItem();

    if(msgFrom->isGroup())
        itemColor = groupColor;
    else
        itemColor = userColor;

    textString = text;
    userString = session->status + msgFrom->textNumber;
    inbox = false;

    // we should not have a disconnected db when we get here...
    if(save) {
        auto storage = Storage::instance();
        Q_ASSERT(storage != nullptr);

        storage->runQuery("INSERT INTO Messages(msgfrom, msgto, sid, seqid, posted, msgtype, msgtext) "
                          "VALUES(?,?,?,?,?,?,?);", {
                              msgFrom->uid,
                              msgTo->uid,
                              sid->contact->uid,
                              dateSequence,
                              dateTime,
                              "TEXT",
                              text
        });
    }
}

MessageItem::MessageItem(const QSqlRecord& record) :
session(nullptr)
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

    auto type = record.value("msgtype").toString();
    if(type == "TEXT") {
        msgType = TEXT_MESSAGE;
        textString = QString::fromUtf8(msgBody);
    }

    userString = session->status + msgFrom->textNumber;
    if(sid == to)
        inbox = false;
    else
        inbox = true;

    if(msgFrom->isGroup())
        itemColor = groupColor;
    else
        itemColor = userColor;
}

QSize MessageItem::layout(const QStyleOptionViewItem& style, int row)
{   
    if(row == 0 || session->filtered[row - 1]->dayNumber != dayNumber)
        dateHint = true;
    else
        dateHint = false;

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

    if(dateHint) {
        QString dateString = dayToday;
        auto today = Util::currentDay();
        if(today != dayNumber) {
            auto date = dateTime.date();
            if(dayNumber + 1 == today)
                dateString = dayYesterday;
            else if(dayNumber + 4 >= today)
                dateString = dayOfWeek[date.dayOfWeek()];
            else
                dateString = dayOfWeek[date.dayOfWeek()] + "   " + monthOfYear[date.month()] + " " + QString::number(date.day());
        }

        textDateline.setTextOption(textCentered);
        textDateline.setTextWidth(96);
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
        textTimestamp.setTextWidth(72);
        textTimestamp.setText(dateTime.time().toString(Qt::DefaultLocaleShortDate).toLower().replace(QString(" "), QString()).replace(QString("m"), QString()));
        textStatus.setText(status + msgFrom->textNumber);
        textDisplay.setText(msgFrom->textDisplay);
        textDisplay.setTextFormat(Qt::RichText);
        userHeight = QFontInfo(style.font).pixelSize() + 4;
        height += userHeight;
    }
    else if(timeHint)
    {
        textTimestamp.setTextOption(textRight);
        textTimestamp.setTextWidth(72);
        textTimestamp.setText(dateTime.time().toString(Qt::DefaultLocaleShortDate).toLower().replace(QString(" "), QString()).replace(QString("m"), QString()));
    }

    textLayout.setFont(textFont);
    textLayout.setText(textString);
    textLayout.setCacheEnabled(true);
    textLayout.setTextOption(textOptions);
    textLayout.setFont(textFont);
    textLayout.beginLayout();

    textHeight = 0;
    for(;;) {
        auto line = textLayout.createLine();
        if(!line.isValid())
            break;

        line.setLeadingIncluded(false);
        line.setLineWidth(width - 48);
        line.setPosition(QPointF(0, textHeight));
        textHeight += line.height();
    }
    height += textHeight + 4;

    if(height)
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
    if(!fast)
        endInsertRows();
    return fast;
}

QModelIndex MessageModel::last()
{
    return index(session->filtered.count() - 1);
}

MessageDelegate::MessageDelegate(QWidget *parent) :
QStyledItemDelegate(parent)
{
    textFont = QGuiApplication::font();

    textFont.setPointSize(textFont.pointSize() + 1);
    boldFont = textFont;
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
        tr("Sunday"),
        tr("Monday"),
        tr("Tuesday"),
        tr("Wednesday"),
        tr("Thursday"),
        tr("Friday"),
        tr("Saturday"),
    };

    monthOfYear = {
        tr("Jan"),
        tr("Feb"),
        tr("Mar"),
        tr("Apr"),
        tr("May"),
        tr("Jun"),
        tr("Jul"),
        tr("Aug"),
        tr("Sep"),
        tr("Oct"),
        tr("Nov"),
        tr("Dec"),
    };

    dayToday = tr("Today");
    dayYesterday = tr("Yesterday");

    textHeight = QFontInfo(textFont).pixelSize();
    monoHeight = QFontInfo(monoFont).pixelSize();
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    auto row = index.row();
    auto session = Sessions::active();

    if(!session || row < 0 || row > session->filtered.count())
        return QSize(0, 0);

    auto item = session->filtered[row];

    if(item->lastHint.width() == style.rect.width())
        return item->lastHint;

    return item->layout(style, row);
}

void MessageDelegate::paint(QPainter *painter, const QStyleOptionViewItem& style, const QModelIndex& index) const
{
    auto row = index.row();
    auto session = Sessions::active();
    auto position = style.rect.topLeft();

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
        painter->setFont(style.font);
        painter->setPen(item->itemColor);
        painter->drawStaticText(userpos, item->textStatus);
        userpos.rx() += 40;
        painter->drawStaticText(userpos, item->textDisplay);
        userpos.setX(style.rect.right() - 72);
        auto font = style.font;
        font.setWeight(10);
        font.setPixelSize(9);
        painter->setFont(font);
        painter->setPen(pen);
        painter->drawStaticText(userpos, item->textTimestamp);
        position.ry() += item->userHeight;
    }
    else if(item->timeHint) {
        position.setX(style.rect.right() - 72);
        auto font = style.font;
        font.setWeight(10);
        font.setPixelSize(9);
        painter->setFont(font);
        painter->drawStaticText(position, item->textTimestamp);
    }

    painter->setFont(textFont);
    position.setX(style.rect.left() + 20);
    item->textLayout.draw(painter, position);
    painter->setFont(style.font);
}
