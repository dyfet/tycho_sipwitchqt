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
    virtual ~CrashHandler() = default;

    static bool corefiles();
    static bool installHandlers();
    NORETURN static void processHandlers();
    
protected:
    CrashHandler();

    virtual void crashHandler() = 0;

private:
    CrashHandler *nextHandler;

    static CrashHandler *HandlerList;
};

/*!
 * Crash handler support.
 * \file crashhandler.hpp
 */

/*!
 * \class CrashHandler
 * \brief Provides a daisy-chain linked list of handlers to call when a crash
 * occurs.  These might include things that save or upload core dumps, or
 * other features to log and process failed application termination.  This is
 * meant to be used as a base class for a specific handler, creating an
 * instance of the class attaches it to the call chain for abnormal
 * termination.
 * \author David Sugar <tychosoft@gmail.com>
 *
 * \fn CrashHandler::CrashHandler()
 * Create and attach a derived class to the termination callback list.
 *
 * \fn CrashHandler::crashHandler()
 * Derived function to actually handle application termination events.
 *
 * \fn CrashHandler::installHandlers()
 * Activates all created crash handler objects for callback on application
 * crash.
 *
 * \fn CrashHandler::corefiles()
 * Enables core files to be saved for this executable if otherwise disabled
 * system-wide.
 */

#endif
