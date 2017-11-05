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

#ifndef __CONSOLE_HPP__
#define __CONSOLE_HPP__

#include <QTextStream>
#include <sstream>
#include <iostream>

class output final : public QTextStream
{
    Q_DISABLE_COPY(output)
public:
    output() : QTextStream(&buffer) {}

    ~output() override;

private:
    QString buffer;
};

class error final : public QTextStream
{
    Q_DISABLE_COPY(error)
public:
    error() : QTextStream(&buffer) {}
    ~error() override;

private:
    QString buffer;
};

class crit final : public QTextStream
{
    Q_DISABLE_COPY(crit)
public:
    crit(int code) : QTextStream(&buffer), exitCode(code) {}
    ~crit() override;

private:
    int exitCode;
    QString buffer;
};



/*!
 * Basic console output support, used before Qt services and logging are started.
 * \file console.hpp
 */

#endif
