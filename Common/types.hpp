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

#ifndef __TYPES_HPP__
#define __TYPES_HPP__

#include "compiler.hpp"
#include <QByteArray>
#include <QString>
#include <QMetaType>

class UString : public QByteArray
{
public:
    UString() : QByteArray("") {}
    UString(const char *string) : QByteArray(string) {}
    UString(const QByteArray& ref) : QByteArray(ref) {}
    UString(const QString& ref) : QByteArray(ref.toUtf8()) {}
    UString(const std::string& ref) : QByteArray(ref.c_str()) {}
    UString(const UString& ref) : QByteArray(ref) {}
    UString(UString&& from) : QByteArray(std::move(from)) {}

    operator std::string() const {
        return std::string(constData());
    }

    UString& operator=(const UString& ref) {
        QByteArray::operator=(ref);
        return *this;
    }

    UString& operator=(UString&& from) {
        QByteArray::operator=(from);
        return *this;
    }

    UString unquote() const;
};

inline UString operator+(const UString& str, char *add) {
    UString result(str);
    result.append(add);
    return result;
}

inline UString operator+(const UString& str, const char *add) {
    UString result(str);
    result.append(add);
    return result;
}

Q_DECLARE_METATYPE(UString)

/*!
 * Some common and widely used types.
 * \file types.hpp
 */

/*!
 * \class UString
 * \brief A more C/C++ friendly string class.
 * Provides a simpler means to bridge the gap between QString's 32 bit
 * unicode, and simple utf8 strings that often are needed to directly
 * call C functions which use pure character functions.
 */

#endif
