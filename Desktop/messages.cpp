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

#include <QWidget>
#include <QVariantHash>
#include <QPen>

#include "phonebook.hpp"
#include "sessions.hpp"
#include "../Connect/storage.hpp"

static int sequence = 0;

MessageItem::MessageItem(SessionItem *sid, const QString& text, const QString& topic) :
session(sid)
{
    dateTime = QDateTime::currentDateTime();
    dayNumber = Util::currentDay(dateTime);
    dateSequence = ++sequence;
    msgType = TEXT_MESSAGE;
    msgSubject = topic.toUtf8();
    msgBody = text.toUtf8();
    msgFrom = Storage::from();
    msgTo = sid->uri();
    inbox = false;
}
