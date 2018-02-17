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

#ifndef CRASHHANDLER_HPP_
#define CRASHHANDLER_HPP_

#include "compiler.hpp"
#include <QtGlobal>

class CrashHandler
{
    Q_DISABLE_COPY(CrashHandler)
public:
    CrashHandler();
    virtual ~CrashHandler() = default;

    static bool corefiles();
    static bool installHandlers();
    NORETURN static void processHandlers();
    
protected:
    virtual void crashHandler() = 0;

private:
    CrashHandler *nextHandler;

    static CrashHandler *HandlerList;
};

/*!
 * Crash handler support for service daemons.
 * \file crashhandler.hpp
 */

#endif
