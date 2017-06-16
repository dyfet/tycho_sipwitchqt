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
#include "system.hpp"
#include "server.hpp"
#include "logging.hpp"

#include <QDir>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

#if defined(UNISTD_SYSTEMD)
#include <systemd/sd-daemon.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <winsock2.h>
#include <tchar.h>
#include <stdio.h>

#include <QAbstractNativeEventFilter>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

#include <csignal>
#include <iostream>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sched.h>
#include <cstring>
#include <cstdio>
#include <climits>
#include <termios.h>
#include <paths.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#endif

#ifdef Q_OS_MAC
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#endif

using namespace std;

#ifdef Q_OS_WIN

extern int main(int argc, char **argv);

class NativeEvent final : public QAbstractNativeEventFilter
{
private:
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) final;
};

bool NativeEvent::nativeEventFilter(const QByteArray &eventType, void *message, long *result) {
    Q_UNUSED(eventType);
    Q_UNUSED(result);

    MSG *msg = static_cast<MSG*>(message);
    switch(msg->message) {
    case WM_CLOSE:
        if(Server::shutdown(15)) {
            qApp->processEvents();
            result = 0l;
            return true;
        }
        break;
    case WM_POWERBROADCAST:
        switch(msg->wParam) {
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMRESUMESUSPEND:
            Server::resume();
            break;
        case PBT_APMSUSPEND:
            Server::suspend();
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return false;
}

static NativeEvent nativeEvents;
static DWORD checkpoint = 1;
static SERVICE_STATUS status;
static SERVICE_STATUS_HANDLE statusHandle = nullptr;

static BOOL WINAPI consoleHandler(DWORD code)
{
    switch(code) {
    case CTRL_LOGOFF_EVENT:
        if(Server::shutdown(14))
            return TRUE;
        break;
    case CTRL_C_EVENT:
        if(Server::shutdown(2))
            return TRUE;
        break;
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
        if(Server::shutdown(3))
            return TRUE;
        break;
    case CTRL_SHUTDOWN_EVENT:
        if(Server::shutdown(9))
            return TRUE;
        break;
    }
    return FALSE;
}

static void WINAPI controlHandler(DWORD event)
{
    switch(event) {
    case SERVICE_CONTROL_STOP:
        Server::shutdown(15);
        return;
    case SERVICE_CONTROL_PAUSE:
        Server::suspend();
        return;
    case SERVICE_CONTROL_CONTINUE:
        Server::resume();
        return;
    default:
        break;
    }
}

namespace System {
    void WINAPI detachService(DWORD argc, LPSTR *argv)
    {
        QByteArray name = QString::fromUtf16((const ushort *)DetachedServices[0].lpServiceName).toLocal8Bit();
        statusHandle = RegisterServiceCtrlHandler(DetachedServices[0].lpServiceName, controlHandler );
        if(statusHandle) {
            System::notifyStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
            main(argc, argv);
        }
        else {
            cerr << "Failed to create service status handle." << endl;
            ::exit(90);
        }
    }

    // windows doesn't use symlinked commands...
    bool isShellAlias(const char *argv0)
    {
        Q_UNUSED(argv0);
        return false;
    }

    bool isControlAlias(const char *argv0)
    {
        Q_UNUSED(argv0);
        return false;
    }

    bool detach(int argc, const char *path, const char *argv0)
    {
        bool detached = false;
        Q_UNUSED(argc);
        Q_UNUSED(argv0);

        if(!QDir::setCurrent(path))
            return false;

        if(!StartServiceCtrlDispatcher(DetachedServices)) {
            auto err = GetLastError();
            if(err != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
                cerr << "Failed service startup." << endl;
                ::exit(90);
            }
            else {
                cout << "Console startup" << endl;
                detached = false;
            }
        }
        else
            ::exit(0);

        if(statusHandle != NULL) {
            detached = true;
        }

        return detached;
    }

    void notifyStatus(SERVICE_STATE state, int exit, int hint, const char *text)
    {
        Q_UNUSED(text);

        if(statusHandle == NULL)
            return;

        status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        status.dwCurrentState = state;
        status.dwWin32ExitCode = exit;
        status.dwWaitHint = hint;

        if(state == SERVICE_START_PENDING)
            status.dwControlsAccepted = 0;
        else
            status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

        if((state == SERVICE_RUNNING) || (state == SERVICE_STOPPED))
            status.dwCheckPoint = 0;
        else
            status.dwCheckPoint = checkpoint++;

        SetServiceStatus(statusHandle, &status);
    }
}
#else

#ifdef UNISTD_SYSTEMD
static bool run_as_systemd = false;
#endif

#ifdef Q_OS_MAC

// FIXME: This will require a new thread for cfrunloop to work!!

static io_connect_t ioroot = 0;
static io_object_t iopobj;
static IONotificationPortRef iopref;

static void iop_callback(void *ref, io_service_t ioservice, natural_t mtype, void *args) {
    Q_UNUSED(ioservice);
    Q_UNUSED(ref);

    switch(mtype) {
    case kIOMessageCanSystemSleep:
        IOAllowPowerChange(ioroot, (long)args);
        break;
    case kIOMessageSystemWillSleep:
        Server::suspend();
        IOAllowPowerChange(ioroot, (long)args);
        break;
    case kIOMessageSystemHasPoweredOn:
        Server::resume();
        break;
    default:
        break;
    }
}

static void iop_startup()
{
    void *ref = nullptr;

    if(ioroot != 0)
        return;

    ioroot = IORegisterForSystemPower(ref, &iopref, iop_callback, &iopobj);
    if(!ioroot) {
        Logging::err() << "Registration for power management failed";
        return;
    }
    else
        Logging::debug() << "Power management enabled";

    CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(iopref), kCFRunLoopCommonModes);
}

#endif

namespace System {
    void notifyStatus(SERVICE_STATE state, int exit, int wait, const char *text)
    {
        Q_UNUSED(wait);
        Q_UNUSED(exit);
#if defined(Q_OS_MAC)
        Q_UNUSED(state);
        Q_UNUSED(text);
#elif defined(UNISTD_SYSTEMD)
        if(!run_as_systemd)
            return;

        const char *cp;
        switch(state) {
        case SERVICE_RUNNING:
            if(!text) {
                cp = "started";
                text = "";
            }
            else
                cp = "started:";

            sd_notifyf(0,
                "READY=1\n"
                "STATUS=%s%s\n"
                "MAINPID=%lu", cp, text, (unsigned long)getpid()
            );
            break;
        case SERVICE_STOPPED:
            sd_notify(0, "STOPPING=1");
            break;
        default:
            break;
        }
#else
        Q_UNUSED(state);
        Q_UNUSED(text);
#endif
    }

    // shell aliases are used to run app specific hash-bang scripts
    bool isShellAlias(const char *argv0)
    {
        const char *alias = nullptr;
        if(argv0) {
            alias = strrchr(argv0, '/');
            if(alias)
                alias = strrchr(alias, '-');
            else
                alias = strrchr(argv0, '-');
        }
        if(!alias)
            return false;

        if(!strcmp(alias, "-sh") || !strcmp(alias, "-shell"))
            return true;

        return false;
    }

    // control aliases are used to directly feed argv to ipc
    bool isControlAlias(const char *argv0)
    {
        const char *alias = nullptr;
        if(argv0) {
            if(strlen(argv0) > 6) {
                alias = argv0 + strlen(argv0) - 3;
                if(!strcmp(argv0, "ctl"))
                    return true;
            }
            alias = strrchr(argv0, '/');
            if(alias)
                alias = strrchr(alias, '-');
            else
                alias = strrchr(argv0, '-');
        }
        if(!alias)
            return false;

        if(!strcmp(alias, "-ctrl") || !strcmp(alias, "-control"))
            return true;

        return false;
    }

    bool detach(int argc, const char *path, const char *argv0)
    {
        mkdir(path, 0770);
        if(chdir(path))
            return false;

        if(isShellAlias(argv0))
            return false;
        if(isControlAlias(argv0))
            return false;

        const char *alias = nullptr;

        // we may optionally use symlinks for special modes...
        if(argv0) {
            alias = strrchr(argv0, '/');
            if(alias)
                alias = strrchr(alias, '-');
            else
                alias = strrchr(argv0, '-');
        }

        if(getppid() == 1 || (alias && !strcmp(alias, "-daemon")) || getenv("NOTIFY_SOCKET") || ((argc < 2) && !getuid())) {
            umask(007);
            if(!detachService()) {
                cerr << "Failed to detach server." << endl;
                exit(90);
            }
            return true;
        }
        return false;
    }

    bool detachService()
    {
        struct rlimit core;

        if(getrlimit(RLIMIT_CORE, &core) != 0) {
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
                cerr << "Cannot set core limit." << endl;
        }

#ifdef UNISTD_SYSTEMD
        if(getenv("NOTIFY_SOCKET")) {
            run_as_systemd = true;
            return true;
        }
#endif
        if(getppid() == 1)
            return true;

        pid_t pid;
        int fd;

        ::close(0);
        ::close(1);
        ::close(2);

#ifdef SIGTTOU
        ::signal(SIGTTOU, SIG_IGN);
#endif

#ifdef SIGTTIN
        ::signal(SIGTTIN, SIG_IGN);
#endif

#ifdef SIGTSTP
        ::signal(SIGTSTP, SIG_IGN);
#endif
        pid = ::fork();
        if(pid > 0)
            ::exit(0);
        if(pid != 0)
            return false;
            
        if((fd = ::open(_PATH_TTY, O_RDWR)) >= 0) {
            ::ioctl(fd, TIOCNOTTY, NULL);
            ::close(fd);
        }

        if(setpgid(0, 0) != 0)
            return false;

        ::signal(SIGHUP, SIG_IGN);
        pid = fork();
        if(pid > 0)
            ::exit(0);
        if(pid != 0)
            return false;
            
        fd = ::open("/dev/null", O_RDWR);
        if(fd > 0)
            ::dup2(fd, 0);
        if(fd != 1)
            ::dup2(fd, 1);
        if(fd != 2)
            ::dup2(fd, 2);
        if(fd > 2)
            ::close(fd);        

        return true;
    }
}
#endif

static void signal_handler(int signo)
{
    switch(signo) {
    case SIGINT:
        printf("\n");
        System::disableSignals();
        Server::shutdown(signo);
        break;
#ifdef SIGKILL
    case SIGKILL:
#endif
    case SIGTERM:
        System::disableSignals();
        Server::shutdown(signo);
        break;
#ifdef SIGHUP
    case SIGHUP:
        Server::reload();
        break;
#endif
    }
}
namespace System {
    unsigned abiVersion()
	{
        QString major = qApp->applicationVersion();
        QString minor = major.mid(major.indexOf('.') + 1);
        major = major.left(major.indexOf('.'));
        minor = minor.left(minor.indexOf('.'));
        unsigned num = major.toUInt() * 100;
        num += minor.toUInt();
        return num;
    }

    void disableSignals()
    {
        ::signal(SIGTERM, SIG_IGN);
        ::signal(SIGINT, SIG_IGN);
#ifdef SIGHUP
        ::signal(SIGHUP, SIG_IGN);
#endif
#ifdef SIGKILL
        ::signal(SIGKILL, SIG_IGN);
#endif
    }

    void enableSignals()
    {
#ifdef Q_OS_WIN
        if(statusHandle == NULL)
            SetConsoleCtrlHandler((PHANDLER_ROUTINE)consoleHandler, TRUE);
        qApp->installNativeEventFilter(&nativeEvents);
#endif

#ifdef Q_OS_MAC
        iop_startup();
#endif

#ifdef SIGHUP
        ::signal(SIGHUP, signal_handler);
#endif
#ifdef SIGKILL
        ::signal(SIGKILL, signal_handler);
#endif
        ::signal(SIGINT, signal_handler);
        ::signal(SIGTERM, signal_handler);
    }
}

