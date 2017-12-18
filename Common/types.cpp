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

#include "compiler.hpp"
#include "types.hpp"

bool UString::isQuoted(const char *qc) const
{
    char q1[2], q2[2];
    q1[1] = q2[1] = 0;
    q1[0] = q2[0] = *qc;

    if(qc[1])
        q2[0] = qc[1];

    if(startsWith(q1) && endsWith(q2))
        return true;

    return false;
}

UString UString::unquote(const char *qc) const
{
    char q1[2], q2[2];
    q1[1] = q2[1] = 0;
    q1[0] = q2[0] = *qc;

    if(qc[1])
        q2[0] = qc[1];

    if(startsWith(q1) && endsWith(q2))
        return mid(1, length() - 2);

    return trimmed();
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
