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

class debug final : public QTextStream
{
    Q_DISABLE_COPY(debug)
public:
    debug() : QTextStream(&buffer) {}

    ~debug() override;

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

class info final : public QTextStream
{
    Q_DISABLE_COPY(info)
public:
    info() : QTextStream(&buffer) {}
    ~info() override;

private:
    QString buffer;
};

class notice final : public QTextStream
{
    Q_DISABLE_COPY(notice)
public:
    notice() : QTextStream(&buffer) {}
    ~notice() override;

private:
    QString buffer;
};

class warning final : public QTextStream
{
    Q_DISABLE_COPY(warning)
public:
    warning() : QTextStream(&buffer) {}
    ~warning() override;

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
 * Basic console output and system logging support.
 * \file output.hpp
 */

#endif
