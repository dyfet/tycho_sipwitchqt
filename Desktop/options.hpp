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

#ifndef OPTIONS_HPP_
#define	OPTIONS_HPP_

#include <QWidget>
#include <QVariantHash>
#include <QButtonGroup>

class Desktop;
class Connector;

class Options : public QWidget
{
public:
    explicit Options(Desktop *main);

    void enter();
    void fontDialog();

private:
    Desktop *desktop;
    QDialog *dialog;
    QString autostartPath;
    Connector *connector;

private slots:
    void secretChanged(const QString& text);
    void changePassword();
    void changeAutostart(int value);
    void changeConnector(Connector *connection);
};

#endif

