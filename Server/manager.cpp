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

#include "../Common/compiler.hpp"
#include "../Common/inline.hpp"
#include "server.hpp"
#include "output.hpp"
#include "manager.hpp"
#include "zeroconf.hpp"
#include "main.hpp"

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <cstdlib>
#include <climits>
#include <cstring>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif
#endif

#include <QUuid>
#include <QJsonObject>

Manager *Manager::Instance = nullptr;
UString Manager::ServerMode;
UString Manager::ServerHostname;
UString Manager::ServerRealm;
UString Manager::ServerBanner = "Welcome to SipWitchQt " PROJECT_VERSION;
QStringList Manager::ServerAliases;
QStringList Manager::ServerNames;
std::atomic<unsigned> Manager::RosterSequence;
unsigned Manager::Contexts = 0;

Manager::Manager(unsigned order)
{
    qRegisterMetaType<Event>("Event");
    qRegisterMetaType<UString>("UString");

    moveToThread(Server::createThread("stack", order));
#ifndef Q_OS_WIN
    osip_trace_initialize_syslog(TRACE_LEVEL0, const_cast<char *>("sipwitchqt"));
#endif

    RosterSequence.store(1, std::memory_order_release);

    Server *server = Server::instance();
    connect(thread(), &QThread::started, this, &Manager::startup);
    connect(thread(), &QThread::finished, this, &QObject::deleteLater);
    connect(server, &Server::changeConfig, this, &Manager::applyConfig);
}

Manager::~Manager()
{
    Instance = nullptr;
}

void Manager::init(unsigned order)
{
    Q_ASSERT(Instance == nullptr);
    Instance = new Manager(order);
}

void Manager::startup()
{
    auto cleanupTimer = new QTimer(this);
    connect(cleanupTimer, &QTimer::timeout, this, &Manager::cleanup);
    cleanupTimer->start(60000l);
}

void Manager::cleanup()
{
    Registry::cleanup();
}

void Manager::applyNames()
{
    QStringList names =  ServerAliases + ServerNames;
    qDebug() << "Apply names" << names;
    foreach(auto context, Context::contexts()) {
        context->applyHostnames(names, ServerHostname);
    }
}

void Manager::applyConfig(const QVariantHash& config)
{
    ServerNames = config["localnames"].toStringList();
    if(Util::isEmpty(ServerNames)) {               // auto add localhost for * case
        ServerNames.clear();
        ServerNames << "127.0.0.1";
        ServerNames << "localhost";
        ServerNames << Util::localName();
        ServerNames << Util::hostName();
#ifdef Q_OS_UNIX
        if(Zeroconfig::enabled())   // avahi published hostname for any client
            ServerNames << Zeroconfig::currentHostName();

        ServerNames << Util::localName() + ".local";
#endif
    }
    if(!config["banner"].toString().isEmpty())
        ServerBanner = config["banner"].toString();
    QString hostname = config["host"].toString();
    QString realm = config["realm"].toString();

    if(realm.isEmpty()) {
        realm = Server::sym(CURRENT_NETWORK);
        if(realm.isEmpty() || realm == "local" || realm == "localhost" || realm == "localdomain")
            realm = Server::uuid();
    }
    if(hostname != ServerHostname) {
        ServerHostname = hostname;
        info() << "starting as host " << ServerHostname;
    }
    if(realm != ServerRealm) {
        ServerRealm = realm;
        info() << "entering realm " << ServerRealm;
        emit changeRealm(ServerRealm);
    }
    applyNames();
}

const QByteArray Manager::computeDigest(const UString& id, const UString& secret, QCryptographicHash::Algorithm digest)
{
    if(secret.isEmpty() || id.isEmpty())
        return QByteArray();

    return QCryptographicHash::hash(id + ":" + realm() + ":" + secret, digest);
}

void Manager::create(const QHostAddress& addr, quint16 port, unsigned mask)
{
    unsigned index = ++Contexts;

    debug() << "Creating sip" << index << ": bind=" <<  addr.toString() << ", port=" << port << ", mask=" << QString("0x%1").arg(mask, 8, 16, QChar('0'));

    foreach(auto schema, Context::schemas()) {
        if(schema.proto & mask) {
            new Context(addr, port, schema, mask, index);
        }
    }
}

void Manager::create(const QList<QHostAddress>& list, quint16 port, unsigned  mask)
{
    foreach(auto host, list) {
        create(host, port, mask);
    }
}

void Manager::dropEndpoint(qlonglong endpoint)
{
    delete Registry::find(endpoint);
}

void Manager::sendMessage(qlonglong endpoint, const QVariantHash& data)
{
    auto *reg = Registry::find(endpoint);
    if(!reg || !reg->isActive()) {
        // will be queued in db only for now...
        qDebug() << "endpoint inactive" << endpoint;
        return;
    }
    auto context = reg->context();
    UString type = data["c"].toByteArray();
    UString from = data["f"].toByteArray();
    UString to = data["t"].toByteArray();
    UString route = context->prefix() + reg->route();
    UString display = data["d"].toByteArray();
    UString topic = data["s"].toByteArray();
    UString label = reg->label();

    if(label == "NONE" && type == "text/admin") {
        type = "text/plain";
        topic = "X-Admin";
    }

    if(label == "NONE" && type != "text/plain") {
        return;
    }

    // adjusts message from and to based on registering entity delivery
    // context-> message(registry, data)...

    if(from.toInt() == reg->extension() || from == reg->user())
        from = context->prefix() + UString::number(reg->extension()) + "@" + reg->origin();
    else if(from.indexOf('@') < 1)
        from = context->prefix() + from + "@" + reg->origin();

    if(to.toInt() == reg->extension() || to == reg->user())
        to = context->prefix() + UString::number(reg->extension()) + "@" + reg->origin();
    else if(to.indexOf('@') < 1)
        to = context->prefix() + to + "@" + reg->origin();

    from = "\"" + display + "\" <" + from + ">";
    to = "<" + to + ">";

    QList<QPair<UString,UString>> headers = {
        {"Subject", topic},
        {"X-MID", data["r"].toString()},
        {"X-EP", QString::number(reg->endpoint())},
        {"X-TS", data["p"].toDateTime().toString(Qt::ISODate)},
        {"X-MS", data["u"].toString()},
    };

    qDebug() << "Sending Message FROM" << from << "TO" << to << "VIA" << route;
    context->message(from, to, route, headers, type, data["b"].toByteArray());
}

void Manager::ackPending(const Event& ev)
{
    qDebug() << "ACK PENDING FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND PENDING REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    Context::reply(ev, result);
    emit changePending(reg->endpoint());
}

void Manager::requestDeauthorize(const Event& ev)
{
    qDebug() << "REQUESTING DEAUTHORIZE FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND DEAUTHORIZE REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit removeAuthorize(ev);
}

void Manager::requestAuthorize(const Event& ev)
{
    qDebug() << "REQUESTING AUTHORIZE FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND AUTHORIZE REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit changeAuthorize(ev);
}


void Manager::requestPending(const Event& ev)
{
    qDebug() << "REQUESTING PENDING FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND PENDING REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit sendPending(ev, reg->endpoint());
}

void Manager::requestDevkill(const Event& ev)
{
    qDebug() << "REQUESTING DEVKILL FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND DEVKILL REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit removeDevice(ev, reg->user(), reg->endpoint());
}


void Manager::requestDevlist(const Event& ev)
{
    qDebug() << "REQUESTING DEVLIST FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND DEVLIST REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit sendDevlist(ev);
}

void Manager::requestTopic(const Event& ev)
{
    qDebug() << "REQUESTING TOPIC FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND TOPIC REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit changeTopic(ev);
}

void Manager::requestForwarding(const Event& ev)
{
    qDebug() << "REQUESTING FORWARDING FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND FORWARDING REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit changeForwarding(ev, reg->user(), reg->endpoint());
}

void Manager::requestCoverage(const Event& ev)
{
    qDebug() << "REQUESTING COVERAGE FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND COVERAGE REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit changeCoverage(ev, reg->user(), reg->endpoint());
}

void Manager::requestDrop(const Event& ev)
{
    qDebug() << "REQUESTING DROP FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND DROP REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit dropExtension(ev, reg->user(), reg->endpoint());
}

void Manager::requestAdmin(const Event& ev)
{
    qDebug() << "REQUESTING ADMIN FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND ADMIN REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit changeAdmin(ev, reg->user(), reg->endpoint());
}

void Manager::requestMembership(const Event& ev)
{
    qDebug() << "REQUESTING MEMBERSHIP FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND MEMBERSHIP REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit changeMembership(ev, reg->user(), reg->endpoint());
}

void Manager::requestProfile(const Event& ev)
{
    qDebug() << "REQUESTING PROFILE FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND PROFILE REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit changeProfile(ev, reg->user(), reg->endpoint());
}

void Manager::requestRoster(const Event& ev)
{
    qDebug() << "REQUESTING ROSTER FROM" << ev.number();
    auto *reg = Registry::find(ev);
    auto result = SIP_FORBIDDEN;

    if(!reg) {
        qDebug() << "CANNOT FIND ROSTER REG";
        Context::reply(ev, result);
        return;
    }

    if(!ev.authorization()) {
        Context::challenge(ev, reg, true);
        return;
    }

    if(SIP_OK != (result = reg->authenticate(ev))) {
        Context::reply(ev, result);
        return;
    }

    emit sendRoster(ev, reg->endpoint());
}

void Manager::refreshRegistration(const Event &ev)
{
    auto *reg = Registry::find(ev);
    if(reg) {
        if(!ev.authorization())
            Context::challenge(ev, reg);
        else {
            auto active = reg->isActive();
            auto result = reg->authorize(ev);
            auto uri = reg->uri();
            if(result == SIP_OK) {
                if(!active)
                    emit lastAccess(reg->endpoint(), ev.timestamp(), ev.agent(), ev.deviceKey(), uri);
                UString xdp;
                if(ev.label() != "NONE") {
                    auto range = Database::range();
                    xdp += "v=0\n";
                    xdp += "b=" + ServerBanner + "\n";
                    xdp += "p=" + reg->privs() + "\n";
                    xdp += "d=" + reg->display() + "\n";
                    xdp += "f=" + UString::number(range.first) + "\n";
                    xdp += "l=" + UString::number(range.second) + "\n";
                    xdp += "r=" + UString::number(static_cast<int>(checkRoster())) + "\n";
                    xdp += "s=" + UString::number(Database::sequence()) + "\n";
                    xdp += "c=" + reg->route() + "\n";
                    xdp += "a=" + Registry::bitmask() + "\n";
                }
                Context::authorize(ev, reg, xdp);
            }
            else
                Context::reply(ev, result);
            // releasing registration expires object
            if(reg->hasExpired())
                delete reg;
        }
    }
    else
        emit findEndpoint(ev);
}

void Manager::createRegistration(const Event& event, const QVariantHash& endpoint)
{
    if(endpoint.isEmpty()) {
        Context::reply(event, SIP_NOT_FOUND);
    }
    else {
        auto reg = new Registry(endpoint);
        Context::challenge(event, reg);
    }
}

