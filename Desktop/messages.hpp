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

#ifndef MESSAGES_HPP_
#define	MESSAGES_HPP_

#include <QWidget>
#include <QVariantHash>
#include <QPen>

class MessageItem final
{
    friend class MessageDelegate;
    friend class MessageModel;

public:
};

class MessageModel final : public QAbstractListModel
{
    friend class MessageDelegate;

public:
    MessageModel(QWidget *parent) : QAbstractListModel(parent) {}
        
private:
    int rowCount(const QModelIndex& parent) const final;
    QVariant data(const QModelIndex& index, int role) const final;
};

#endif

