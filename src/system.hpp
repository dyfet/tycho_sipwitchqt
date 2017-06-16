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

#include <QtGlobal>

#ifdef Q_OS_UNIX
typedef enum {
	SERVICE_START_PENDING,
	SERVICE_RUNNING,
	SERVICE_STOP_PENDING,
	SERVICE_STOPPED
} SERVICE_STATE;
#define NO_ERROR 0
#else
#include <windows.h>
typedef DWORD SERVICE_STATE;
extern SERVICE_TABLE_ENTRY DetachedServices[];
#endif

namespace System {
#ifdef Q_OS_UNIX
    bool detachService();
#endif
#ifdef Q_OS_WIN
    void WINAPI detachService(DWORD argc, LPSTR *argv);
#endif
    bool isShellAlias(const char *argv0);
    bool isControlAlias(const char *argv0);
    bool detach(int argc, const char *path, const char *argv0 = nullptr);
    unsigned abiVersion();
    void notifyStatus(SERVICE_STATE state, int exit = NO_ERROR, int wait = 0, const char *text = nullptr);
    void disableSignals(void);
    void enableSignals(void);
};

