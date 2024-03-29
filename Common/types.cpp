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

#include "compiler.hpp"
#include "types.hpp"
#include <cctype>

bool UString::toBool() const
{
    auto cp = constData();
    if(!cp || *cp == 0)
        return false;

    if(toLower() == "yes" || toLower() == "true")
        return true;

    return atoi(cp) > 0;
}

bool UString::isNumber() const
{
    auto cp = constData();

    if(!*cp)
        return false;

    if(*cp == '0' && !cp[1])
        return true;

    while(*cp) {
        if(!isdigit(*(cp++)))
            return false;
    }

    return true;
}

bool UString::isLabel() const
{
    auto cp = constData();
    if(!*cp)
        return false;

    if(!isalpha(*cp))
        return false;

    while(*cp) {
        if(!isalnum(*(cp++)))
            return false;
    }
    return true;
}

bool UString::isQuoted(const char *qc) const
{
    char q1[2], q2[2];
    q1[1] = q2[1] = 0;
    q1[0] = q2[0] = *qc;

    if(qc[1])
        q2[0] = qc[1];

    return startsWith(q1) && endsWith(q2);
}

UString UString::unescape() const
{
    UString result;
    char digits[3];
    auto cp = constData();
    while(cp && *cp) {
        if(*cp == '%') {
            digits[0] = *(++cp);
            if(!digits[0])
                break;
            digits[1] = *(++cp);
            if(!digits[1])
                break;
            digits[2] = 0;
            UString value(digits);
            auto code = static_cast<char>(value.toInt(nullptr, 16));
            result.append(code);
            ++cp;
        }
        else
            result.append(*(cp++));

    }
    return result;
}

UString UString::escape() const
{
    UString result;
    for(int pos = 0; pos < length(); ++pos) {
        auto ch = at(pos);
        if(ch == ' ')
            result += "%20";
        else if(ch == '&')
            result += "%26";
        else if(ch == '<')
            result += "%3c";
        else if(ch == '>')
            result += "%3e";
        else if(ch == '@')
            result += "%40";
        else if(ch == '%')
            result += "%25";
        else if(ch == ':')
            result += "%3a";
        else
            result += ch;
    }
    return result;
}

UString UString::unquote(const char *qc) const
{
    char q1[2], q2[2];
    q1[1] = q2[1] = 0;
    q1[0] = q2[0] = *qc;

    if(qc[1])
        q2[0] = qc[1];

    return startsWith(q1) && endsWith(q2) ? mid(1, length() - 2) : trimmed(); // NOLINT

}

UString UString::quote(const char *qc) const
{
    char q1 = qc[0], q2 = qc[0];
    if(qc[1])
        q2 = qc[1];
    UString quoted;
    quoted.append(q1);
    quoted.append(constData());
    quoted.append(q2);
    return quoted;
}

UString UString::uri(const UString& schema, const UString& server, quint16 port)
{
    auto prefix = schema.toLower();
    auto host = server.unquote("[]");

    if(prefix == "sip:" && port == 5060)
        port = 0;
    if(prefix == "sips:" && port == 5061)
        port = 0;

    UString suffix = "";
    if(port > 0)
        suffix = ":" + UString::number(port);

    if(host.indexOf(":") > -1)
        host = "[" + host + "]";

    return prefix + host + suffix;
}

UString UString::uri(const UString& schema, const UString& id, const UString& server, quint16 port)
{
    auto prefix = schema.toLower();
    auto host = server.unquote("[]");

    if(prefix == "sip:" && port == 5060)
        port = 0;
    if(prefix == "sips:" && port == 5061)
        port = 0;

    UString suffix = "";
    if(port > 0)
        suffix = ":" + UString::number(port);

    if(host.indexOf(":") > -1)
        host = "[" + host + "]";

    return prefix + id + "@" + host + suffix;
}

