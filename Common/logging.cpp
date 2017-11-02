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
#include "server.hpp"
#include "logging.hpp"

#include <QDate>
#include <QTime>
#include <QTimer>
#include <ctime>
#include <iostream>
#include <unistd.h>
#include <syslog.h>

using namespace std;

class LoggingEvent final : public QEvent
{
    Q_DISABLE_COPY(LoggingEvent)

public:
    LoggingEvent(Logging::Level event, QByteArray msg) :
    QEvent(static_cast<QEvent::Type>(event)) {
        message = msg;
        time(&timestamp);
    }

    ~LoggingEvent();

    inline time_t when() const {
        return timestamp;
    }

    inline const char *text() const {
        return message.constData();
    }

private:
    QByteArray message;
    time_t timestamp;
};

LoggingEvent::~LoggingEvent() {}

static bool up = false;
static bool service = false;
static QTimer timer;
static QTime last;
static FILE *fp = nullptr;
static const char *logfile;

#if !defined(DEBUG_LOGGING)
static void report(Logging::Level logtype, const char *text)
{
    if(!up)
        return;

    switch(logtype) {
    case Logging::LOGGING_ERROR:
        syslog(LOG_ERR, "%s", text);
        break;
    case Logging::LOGGING_INFO:
        syslog(LOG_INFO, "%s", text);
        break;
    case Logging::LOGGING_NOTICE:
        syslog(LOG_NOTICE, "%s", text);
        break;
    case Logging::LOGGING_WARN:
        syslog(LOG_WARNING, "%s", text);
        break;
    case Logging::LOGGING_FATAL:
        syslog(LOG_CRIT, "%s", text);
        break;
    case Logging::LOGGING_IGNORE:
        return;
    }
}
#endif


Logging *Logging::Instance = nullptr;

Logging::Logging() :
QObject(Server::instance())
{                    
    Q_ASSERT(Instance == nullptr);
    Instance = this;

    Server *server = Server::instance();
    connect(server, &Server::aboutToStart, this, &Logging::onStartup);
    connect(server, &Server::finished, this, &Logging::onShutdown);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Logging::onExiting);
    qInstallMessageHandler(logHandler);
}

Logging::~Logging()
{
    qInstallMessageHandler(0);
    Instance = nullptr;
}

bool Logging::event(QEvent *ev)
{
    int id = static_cast<int>(ev->type());
    if(id < LOGGING_INFO)
        return QObject::event(ev);

    auto evt = static_cast<LoggingEvent*>(ev);
    Q_ASSERT(evt != nullptr);

    const char *category = nullptr;
    const char *text = evt->text();
    const char *msg = text;

    if(ispunct(*msg) || isspace(*msg))
        msg += 3;

    Level level = static_cast<Level>(id);
    switch(level) {
    case LOGGING_ERROR:
        category = "error";
        break;
    case LOGGING_INFO:          // info can carry server debug...
        category = "info";
        break;
    case LOGGING_NOTICE:
        category = "notice";
        break;
    case LOGGING_WARN:
        category = "warn";
        if(*text == ' ') {
            level = LOGGING_IGNORE;
            category = "debug";
            text += 3;
        }
        if(*text == '!') {
            level = LOGGING_NOTICE;
            category = "notice";
            break;
        }
        break;
    case LOGGING_FATAL:
        category = "fatal";
        break;
    case LOGGING_IGNORE:        // should not be passed here...
        return true;
    }

    // squash server debug if not server verbose
    if(level == LOGGING_IGNORE && !Server::verbose())
        return true;

#if !defined(DEBUG_LOGGING)
    if(service && category) {
        if(level != LOGGING_IGNORE)
            report(level, msg);
        //category = nullptr;
    }
#endif

    if(category)
        append(category, msg);

    if(!service) {
        cerr << text << endl;
        cerr.flush();
    }

    if(level != LOGGING_IGNORE)
        emit notify(level, evt->when(), msg);

    return true;
}

QDebug Logging::info(const char *prefix)
{
    QDebug log = qInfo().noquote().nospace();
    log << prefix << " ";
    return log;
}

QDebug Logging::warn(const char *prefix) {
    QDebug log = qWarning().noquote().nospace();
    log << prefix << " ";
    return log;
}

QDebug Logging::err(const char *prefix) {
    QDebug log = qCritical().noquote().nospace();
    log << prefix << " ";
    return log;
}

QDebug Logging::crit(int reason) {
    QDebug log = qCritical().noquote().nospace();
    log << reason << " ";
    return log;
}

void Logging::onTimeout()
{
    if(fp)
        fflush(fp);

    if(fp && last.elapsed() > 10000)
        close();
}

void Logging::onStartup()
{
    connect(&timer, &QTimer::timeout, this, &Logging::onTimeout);
    service = Server::isService();
    qDebug() << "Starting logger";

    if(service)
        ::openlog(Server::name(), LOG_CONS, LOG_DAEMON);

    last.start();
    logfile = Server::sym(SERVER_LOGFILE);
    up = true;

    // if we are using local test directory, start with clean log each run...
#ifdef DEBUG_LOGGING
    ::remove(logfile);
#endif
}

void Logging::onShutdown()
{
    timer.stop();
    up = false;
    service = false;
    close();
    ::closelog();
    qDebug() << "Stopping logger";
}

void Logging::onExiting()
{
    if(up)
        onShutdown();

    delete Instance;
}

void Logging::append(const char *category, const char *text)
{
    if(!up)
        return;

    if(!fp) {
        fp = fopen(logfile, "a");
        if(!fp) {
            up = false;
            Logging::crit(91) << logfile << ": " << tr("cannot append log");
            return;
        }
        if(up) {
            qDebug() << "Logging(OPEN)";
            timer.setInterval(1000);
            timer.start();
        }
    }

    QDate now = QDate::currentDate();
    last.start();
    if(last.hour() == 0 && last.minute() == 0 && last.second() < 3)
        now = QDate::currentDate();

    fprintf(fp, "%04d-%02d-%02d %02d:%02d:%02d %s: %s\n",
        now.year(), now.month(), now.day(),
        last.hour(), last.minute(), last.second(),
        category, text);

    if(!up) {
        fclose(fp);
        fp = nullptr;
    }
}

void Logging::close()
{
    timer.stop();
    if(fp) {
        qDebug() << "Logging(CLOSE)";
        fclose(fp);
        fp = nullptr;
    }
}

void Logging::logHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    auto logtype = Logging::LOGGING_INFO;
    auto priority = Qt::NormalEventPriority;

    Q_UNUSED(context);
    Q_ASSERT(Instance != nullptr);

    QByteArray localMsg = msg.toLocal8Bit();
    const char *text;
    static int exit_reason = 0;

    switch(type) {
    case QtDebugMsg:
#ifdef QT_NO_DEBUG_OUTPUT
        Q_UNUSED(text);
#else
        text = localMsg.constData();
        cerr << text << endl;
        cerr.flush();
#endif
        return;
    case QtInfoMsg:
        logtype = Logging::LOGGING_INFO;
        break;
    case QtWarningMsg:
        logtype = Logging::LOGGING_WARN;
        break;
    case QtCriticalMsg:
        logtype = Logging::LOGGING_ERROR;
        if(msg[0].isDigit() && msg[1].isDigit() && msg[2].isSpace()) {
            priority = Qt::HighEventPriority;
            logtype = Logging::LOGGING_FATAL;
            exit_reason = atoi(localMsg.constData());
            if(up && service)
                localMsg[0] = localMsg[1] = '!';
            else
                localMsg[0] = localMsg[1] = '*';
        }
        break;
    case QtFatalMsg:
        logtype = Logging::LOGGING_FATAL;
        priority = Qt::HighEventPriority;
        break;
    }

    if(!up) {
        Instance->event(new LoggingEvent(logtype, localMsg));
        if(exit_reason)
            ::exit(exit_reason);
        return;
    }
    QCoreApplication::postEvent(Instance, new LoggingEvent(logtype, localMsg), priority);
    if(exit_reason)
        Server::shutdown(exit_reason);
}
