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

#include "crashhandler.hpp"

#include <QtDebug>

#include <csignal>
#include <execinfo.h>
#include <cxxabi.h>
#include <signal.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/resource.h>

static bool installed = false;

CrashHandler *CrashHandler::HandlerList = nullptr;

CrashHandler::CrashHandler()
{
    if(installed)
        abort();

    nextHandler = HandlerList;
    HandlerList = this;
}

void CrashHandler::processHandlers()
{
    while(HandlerList) {
        HandlerList->crashHandler();
        HandlerList = HandlerList->nextHandler;
    }
    abort();
}

#if defined(QT_NO_DEBUG)

static bool processing = false;

static void handler(int sig, siginfo_t *siginfo, void *context)
{
    Q_UNUSED(context);
    Q_UNUSED(sig);
    Q_UNUSED(siginfo);

    if(processing)
        return;

    processing = true;
    CrashHandler::processHandlers();
}

bool CrashHandler::corefiles(void)
{
    struct rlimit core;

    if(getrlimit(RLIMIT_CORE, &core) != 0)
        return false;
#ifdef  MAX_CORE_SOFT
    core.rlim_cur = MAX_CORE_SOFT;
#else
    core.rlim_cur = RLIM_INFINITY;
#endif
#ifdef  MAX_CORE_HARD
    core.rlim_max = MAX_CORE_HARD;
#else
    core.rlim_max = RLIM_INFINITY;
#endif
    if(setrlimit(RLIMIT_CORE, &core) != 0)
        return false;

    return true;
}

bool CrashHandler::installHandlers()
{
    if(installed)
        return false;

    installed = true;
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handler;
    sigemptyset( &sa.sa_mask );

    sigaction( SIGSEGV, &sa, NULL );
    sigaction( SIGBUS,  &sa, NULL );
    sigaction( SIGILL,  &sa, NULL );
    sigaction( SIGFPE,  &sa, NULL );
    sigaction( SIGPIPE, &sa, NULL );
    return true;
}

#else

bool CrashHandler::installHandlers()
{
    return false;
}

bool CrashHandler::corefiles()
{
    return false;
}


#endif


