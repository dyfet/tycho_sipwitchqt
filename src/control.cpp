/**
 ** Copyright 2017 Tycho Softworks.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "compiler.hpp"
#include "control.hpp"
#include "server.hpp"
#include "logging.hpp"
#include "crashhandler.hpp"
#include <iostream>
#include <QFile>
#include <QDir>
#include <QLocalSocket>
#include <QDataStream>
#include <QLockFile>
#include <QUuid>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace std;

Control *Control::Instance = nullptr;

Control::Control() :
QObject(), localServer(nullptr)
{
    init();
}

Control::Control(const char *path) :
QObject(), settings(path, QSettings::IniFormat), localServer(nullptr)
{
    init();
}

Control::~Control()
{
    settings.sync();
}

void Control::init()
{
    if(settings.value("env/uuid").isNull()) {
        QString uuid = QUuid::createUuid().toString();
        uuid = uuid.mid(2);
        uuid.chop(1);
        settings.setValue("env/uuid", uuid);
    }

    Q_ASSERT(Instance == nullptr);
    Instance = this;

    QSettings bootstrap(":/etc/bootstrap.conf");
    auto keys = bootstrap.allKeys();
    foreach(auto key, keys) {
        if(settings.value(key).isNull())
            settings.setValue(key, bootstrap.value(key));
    }

    Server *server = Server::instance();
    connect(server, &Server::started, this, &Control::onStartup);
    connect(server, &Server::finished, this, &Control::onShutdown);
    connect(server, &Server::aboutToStart, this, &Control::processConfig);
}

void Control::processConfig()
{
#if defined(Q_OS_MAC)
    QString path = QDir::currentPath() + "/control";
    QFile::remove(path);
#elif defined(Q_OS_UNIX)
    QString path = QString(".") + Server::name() + "-" + QString::number(getuid());
    QFile::remove(QDir::cleanPath(QDir::tempPath()) + QLatin1Char('/') + path);
#else
    QString path = Server::name();
#endif
    localServer = new QLocalServer(this);
    localServer->setSocketOptions(QLocalServer::GroupAccessOption | QLocalServer::UserAccessOption);
    if(!localServer->listen(path)) {
        cerr << "Cannot create control " << path.toStdString() << "." << endl;
        ::exit(90);
    }
    connect(localServer, &QLocalServer::newConnection, this, &Control::acceptConnection);

    qDebug() << "Post settings to slots";

    foreach(auto key, settings.allKeys()) {
        int pos = key.indexOf("/");
        if(pos < 1)
            continue;
        QString prefix = key.left(pos);
        QString id = key.mid(++pos);
        if(prefix == "env") {
            setServer(id.toUpper(), settings.value(key));
        }
        else if(prefix == "values") {
            emit changeValue(id, settings.value(key));
        }
    }
}

void Control::reset(const char *settingsPath)
{
    QByteArray name = Server::name();
    QLockFile pidfile(Server::sym(SERVER_PIDFILE));
    if(!pidfile.tryLock()) {
        std::cerr << "Another instance is running." << std::endl;
        ::exit(90);
    }
    ::remove(settingsPath);
    ::exit(0);
}

bool Control::setValue(const QString& key, const QVariant& value)
{
    QLockFile pidfile(Server::sym(SERVER_PIDFILE));
    if(!pidfile.tryLock())
        return false;
    settings.setValue("values/" + key, value);
    settings.sync();
    return true;
}

void Control::requestKeys(const QString& id)
{
    QVariantHash list;
    settings.beginGroup(id);
    auto keys = settings.childKeys();
    foreach(auto key, keys)
        list[key] = settings.value(key);
    settings.endGroup();
    emit publishKeys(list);
}

void Control::updateKeys(const QString& id, const QVariantHash& list)
{
    settings.remove(id);
    settings.beginGroup(id);
    foreach(auto key, list.uniqueKeys()) {
        settings.setValue(key, list[key]);
    }
    settings.endGroup();
    settings.sync();
}

const QString Control::execute(const QStringList& args)
{
    if(args.size() > 1) {
        return "2:invalid arguments";
    }
    if(args[0] == "status") {
        switch(Server::state()) {
        case Server::START:
            return "0:starting";
        case Server::UP:
            return "0:online";
        case Server::DOWN:
            return "0:stopping";
        case Server::SUSPENDED:
            return "0:suspended";
        }
    }
    if(args[0] == "reload") {
        Server::reload();
    }
    else if(args[0] == "stop" || args[0] == "shutdown") {
        Server::shutdown(0);
    }
    else if(args[0] == "suspend") {
        if(Server::state() == Server::SUSPENDED)
            return "3:already suspended";
        Server::suspend();
    }
    else if(args[0] == "resume") {
        if(Server::state() != Server::SUSPENDED)
            return "3:not suspended";
        Server::resume();
    }
    else if(args[0] == "abort") {
        CrashHandler::processHandlers();
    }
    else
        return "1:invalid command";
    return "0";
}

void Control::storeValue(const QString& id, const QVariant& value)
{
    QString key = "values/" + id;
    if(settings.value(key) == value)
        return;

    settings.setValue(key, value);
    settings.sync();
    emit changeValue(id, value);
}

bool Control::setServer(const QString& id, const QVariant& value, bool override)
{
    Server *server = Server::instance();
    if(!server || Server::state() != Server::START)
        return false;

    if(!override && !server->Env[id].isNull())
        return false;

    server->Env[id] = value.toString().toUtf8().constData();
    return true;
}

void Control::acceptConnection()
{
    QLocalSocket* socket = localServer->nextPendingConnection();
    if (!socket)
        return;

    connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    while (socket->bytesAvailable() < static_cast<int>(sizeof(quint32))) {
        if(!socket->isValid())
            return;

        socket->waitForReadyRead(1000);
    }
    QDataStream ds(socket);
    QByteArray msg;
    quint32 remaining;
    ds >> remaining;
    int got = 0;
    msg.resize(static_cast<int>(remaining));
    char *cp = msg.data();
    do {
        got = ds.readRawData(cp, static_cast<int>(remaining));
        if(got < 0)
            break;
        remaining -= static_cast<quint32>(got);
        cp += got;
    } while(remaining && got >= 0 && socket->waitForReadyRead(2000));
    if(got < 0) {
        Logging::warn() << "control receive failed";
        socket->deleteLater();
        return;
    }
    qDebug() << "Requested" << msg;
    if(Server::state() == Server::DOWN)
        msg = "3:server exiting";
    else
        msg = execute(QString::fromUtf8(msg).split(";")).toUtf8();
    ds.writeBytes(msg.constData(), static_cast<unsigned>(msg.size()));
    socket->flush();
    socket->waitForBytesWritten(1000);
    socket->disconnectFromServer();
    socket->deleteLater();
}

int Control::command(const QString& arg, bool verbose, int timeout)
{
    QStringList args;
    args << arg;
    int result = request(args, verbose, timeout);
    if(result == -1)
        cerr << "Server is offline." << endl;
    else if(result == -2)
        cerr << "Server control failed." << endl;
    return result;
}

int Control::request(const QStringList& args, bool verbose, int timeout)
{
#if defined(Q_OS_MAC)
    QString path = QDir::currentPath() + "/control";
#elif defined(Q_OS_UNIX)
    QString path = QString(".") + Server::name() + "-" + QString::number(getuid());
#else
    QString path = Server::name();
#endif
    QLocalSocket socket;
    bool connected = false;
    for(unsigned i = 0; i < 2; ++i) {
        socket.connectToServer(path);
        connected = socket.waitForConnected(timeout/2);
        if(connected || i)
            break;
        int ms = 250;
#if defined(Q_OS_WIN)
        Sleep(DWORD(ms));
#else
        struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
        nanosleep(&ts, NULL);
#endif
    }
    if(!connected) {
        if(verbose)
            Logging::err() << "cannot connect to control interface";
        return -1;
    }

    QByteArray msg(args.join(";").toUtf8());
    QDataStream ds(&socket);
    ds.writeBytes(msg.constData(), static_cast<unsigned>(msg.size()));
    bool result = socket.waitForBytesWritten(timeout);
    if(result) {
        while (socket.bytesAvailable() < static_cast<int>(sizeof(quint32))) {
            socket.waitForReadyRead(1000);
        }
        quint32 remaining;
        ds >> remaining;
        msg.resize(static_cast<int>(remaining));
        int got = 0;
        char *cp = msg.data();
        do {
            got = ds.readRawData(cp, static_cast<int>(remaining));
            if(got < 0)
                break;
            remaining -= static_cast<quint32>(got);
            cp += got;
        } while(remaining && got >= 0 && socket.waitForReadyRead(2000));
        if(got < 0) {
            if(verbose)
                Logging::warn() << "control request failed: " << socket.errorString();
            return -2;
        }
        int pos = msg.indexOf(":");
        if(pos > 0) {
            QTextStream out(stdout);
            out << msg.mid(++pos);
            out << "\n";
            out.flush();
        }
        return atoi(msg.constData());
    }
    if(verbose)
        Logging::err() << "control request failed";
    return -1;
}

void Control::options(QCommandLineParser& args)
{
    QTextStream out(stdout);
    if(args.isSet("control"))
        ::exit(request(args.positionalArguments()));
    else if(args.isSet("status"))
        ::exit(command("status"));
    else if(args.isSet("reload"))
        ::exit(command("reload"));
    else if(args.isSet("shutdown"))
        ::exit(command("shutdown"));
    else if(args.isSet("abort"))
        ::exit(command("abort"));
    else if(args.isSet("show-env")) {
        processConfig();
        auto env = Server::env();
        QStringList keys = env.keys();
        keys.sort();
        foreach(auto key, keys) {
            out << key << "=" << env[key] << endl;          
        }
        out.flush();
        ::exit(0);
    }
    else if(args.isSet("show-config")) {
        QVariantHash cfg = Server::CurrentConfig;
        QStringList keys = cfg.keys();
        keys.sort();
        foreach(auto key, keys) {
            out << key << "=" << cfg[key].toString() << endl;
        }
        out.flush();
        ::exit(0);
    }
    else if(args.isSet("show-values")) {
        QStringList keys = settings.allKeys();
        keys.sort();
        foreach(auto key, keys) {
            int pos = key.indexOf("/");
            if((pos > 1) && (key.left(pos) == "values")) {
                QString id = key.mid(++pos);
                out << id << "=" << settings.value(key).toString() << endl;
            }
        }
        out.flush();
        ::exit(0);
    }
}

