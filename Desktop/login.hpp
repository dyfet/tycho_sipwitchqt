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

#ifndef LOGIN_HPP_
#define	LOGIN_HPP_

#include <QWidget>
#include <QVariantHash>

class Desktop;

class Login : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(Login)

public:
    explicit Login(Desktop *main);

    void enter();
    void badIdentity();
    void badPassword();
    void badLabel();

    QVariantHash credentials();     // gathers local ones...

private:
    Desktop *desktop;
};

#endif

