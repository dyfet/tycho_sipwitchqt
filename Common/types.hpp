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

#ifndef TYPES_HPP_
#define TYPES_HPP_

#include "compiler.hpp"
#include <QByteArray>
#include <QString>
#include <QMetaType>

class UString : public QByteArray
{
public:
    UString() noexcept : QByteArray("") {}
    UString(const char *string) noexcept : QByteArray(string) {}
    UString(const QByteArray& ref) noexcept : QByteArray(ref) {}
    UString(const QString& ref) noexcept : QByteArray(ref.toUtf8()) {}
    UString(const std::string& ref) noexcept : QByteArray(ref.c_str()) {}
    UString(const char ch) noexcept : QByteArray() { append(ch); }
    UString(UString&& from) noexcept: QByteArray(std::move(from)) {}
    UString(const UString& ref) noexcept = default;

    explicit operator std::string() const {
        return std::string(constData());
    }

    UString& operator=(const UString& ref) = default;

    UString& operator=(UString&& from) noexcept {
        QByteArray::operator=(from);
        return *this;
    }

    UString toLower() const {
        return QByteArray::toLower();
    }

    UString toUpper() const {
        return QByteArray::toUpper();
    }

    bool isNumber() const;
    bool isLabel() const;
    UString unquote(const char *qc = "\"") const;
    UString quote(const char *qc = "\"") const;
    UString escape() const;
    bool isQuoted(const char *qc = "\"") const;

    static UString number(int num, int base = 10) {
        return QByteArray::number(num, base);
    }

    static UString number(uint num, int base = 10) {
        return QByteArray::number(num, base);
    }

    static UString number(qlonglong num, int base = 10) {
        return QByteArray::number(num, base);
    }

    static UString number(qulonglong num, int base = 10) {
        return QByteArray::number(num, base);
    }

    static UString number(double num, char fmt = 'g', int prec = 6) {
        return QByteArray::number(num, fmt, prec);
    }

    static UString uri(const UString& schema, const UString& server, quint16 port);
    static UString uri(const UString& schema, const UString& id, const UString& server, quint16 port);
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

inline UString operator+(const UString& str, const std::string& add) {
    UString result(str);
    result.append(add.c_str());
    return result;
}

inline UString operator+(const std::string& str, const UString& add) {
    UString result(str.c_str());
    result.append(add);
    return result;
}

inline UString operator+(const UString& str, const UString& add) {
    UString result(str);
    result.append(add);
    return result;
}

inline UString operator+(const char *str, const UString& add) {
    UString result(str);
    result.append(add);
    return result;
}

inline QString operator+(const QString& str, const UString& add) {
    QString result(str);
    result.append(add);
    return result;
}

inline QString operator+(const UString& str, const QString& add) {
    QString result(str.constData());
    result.append(add.toUtf8());
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
 * Provides a simpler means to bridge the gap between QString's utf16
 * encoded text, and simple utf8 strings that often are needed to directly
 * call C functions which will use 8 bit/utf8 encoded char strings.
 * \author David Sugar <tychosoft@gmail.com>
 *
 * \fn UString::UString()
 * Create an empty string.
 *
 * \fn UString::UString(const char *string)
 * Create a UString to hold a copy/cache of an existing C string.  This is
 * commonly used because UString has managed shared memory for the text.
 * Hence the C string stored in a UString becomes pointer safe, and the
 * original can be free'd.
 * \param string to cache in UString object.
 *
 * \fn UString::UString(const QByteArray& bytes)
 * Promote a QByteArray to a UString.
 * \param bytes Array to make into a string.
 *
 * \fn UString::UString(const std::string& string)
 * Convert and copy a C++ string into a UString object.
 * \param string C++ string to convert.
 *
 * \fn UString::UString(char code)
 * Convert an 8 bit C character value into a single character string.
 * \param code Character code to make into string.
 *
 * \fn UString::toLower()
 * Creates a lower case copy of the string.
 * \return lower case string.
 *
 * \fn UString::toUpper()
 * Creates an upper case copy of the string.
 * \return upper case string.
 *
 * \fn UString::isNumber()
 * Tests if string is a pure integer number.
 * \return true if string text is a integer number.
 *
 * \fn UString::isLabel()
 * Tests if string begins with alpha text and is alphanumeric.
 * \return true if string text is a valid label.
 *
 * \fn UString::isQuoted(const char *qc)
 * Tests if string is quoted.  A quote can be matching quotes, such as "'",
 * or a start and end, such as "()" or "<>".
 * \param qc Quote to check, either matched or paired.
 * \return true if string is quoted.
 *
 * \fn UString::unquote(const char *qc)
 * Removes lead and trailing quotation marks.  A quotation can be encoded
 * in matching quotes, such as "'", or a start and end pair, such as "()",
 * "<>", or "[]", for example.
 * \param qc Quote to remove, either matched or paired.
 * \return new string stripped of quotes.
 *
 * \fn UString::quote(const char *qc)
 * Creates a quoted version of the strng.  A quotation can be encoded
 * in matching quotes, such as "'", or a start and end pair, such as "()",
 * "<>", or "[]", for example.
 * \param qc Quote to add, either matched or paired.
 * \return quoted version of string.
 *
 * \fn UString::escape()
 * Creates a url escaped version of the text string.  These are typically
 * used to create url safe text fields to pass to a web server or as an
 * argument to a sip message.  Hence, control characters and special characters
 * like <,>,/,%, etc, will be escaped into a hex code preceeded by %.
 * \return url safe string.
 *
 * \fn UString::number(int value, int base)
 * Convert an integer into a utf8 text string.  By default base 10 conversion
 * is used.
 * \param value Integer to convert.
 * \param base Number base to use, 10 by default.
 * \return string object for number.
 *
 * \fn UString::uri(const UString& schema, const UString& server, quint16 port)
 * Creates a sip url from a server address and port.  Either sip: or sips:
 * schema can be used.  Also quotes server in [] for ipv6 addresses.
 * \param schema Either sip: or sips:, others maybe in future.
 * \param server Host name or address of server.
 * \param port Internet port number of service on server.
 * \return string as sip server uri, "sip[s]:host:port".
 *
 * \fn UString::uri(const UString& schema, const UString& id, const UString& server, quint16 port)
 * Create a sip uri for a specific user.  Either sip: or sips: schema can be
 * used.  Also quotes server in [] for ipv6 addresses.
 * \param schema Either sip: or sips:, maybe mailto: and callto: in future.
 * \param id Identity of user.
 * \param server Host name or address of server.
 * \param port Internet port number of service to contact.
 * \return string as sip uri, "sip[s]:user@host:port".
 *
 */

#endif
