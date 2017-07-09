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

#include "context.hpp"

Event::Data::Data() :
context(nullptr), event(nullptr), association(NONE)
{
}

Event::Data::Data(eXosip_event_t *evt, Context *ctx) :
context(ctx), event(evt), association(NONE)
{
}

Event::Data::Data(const Data &copy) :
QSharedData(copy)
{
}

Event::Data::~Data()
{
    if(event) {
        eXosip_event_free(event);
        event = nullptr;
    }
}

Event::Event()
{
    d = new Event::Data();
}

Event::Event(eXosip_event_t *evt, Context *ctx)
{
    d = new Event::Data(evt, ctx);
}

Event::Event(const Event& copy) :
d(copy.d)
{
}

Event::~Event()
{
}

const QString Event::protocol() const
{
    Q_ASSERT(d->context != nullptr);
    return d->context->type();
}

QDebug operator<<(QDebug dbg, const Event& ev)
{
    if(ev)
        dbg.nospace() << "Event(" << ev.toString() << ",cid=" << ev.cid() << ",did=" << ev.did() << ",proto=" << ev.protocol() << ")";
    else
        dbg.nospace() << "Event()";
    return dbg.maybeSpace();
}

const QString Event::toString() const
{
    if(!d->event)
        return "none";

    QString result = QString::number(d->event->type);
    switch(d->event->type) {
    case EXOSIP_REGISTRATION_SUCCESS:
        return result + "/register";
    case EXOSIP_CALL_INVITE:
        return result + "/invite";
    case EXOSIP_CALL_REINVITE:
        return result + "/reinvite";
    case EXOSIP_CALL_NOANSWER:
    case EXOSIP_SUBSCRIPTION_NOANSWER:
    case EXOSIP_NOTIFICATION_NOANSWER:
        return result + "/noanswer";
    case EXOSIP_MESSAGE_PROCEEDING:
    case EXOSIP_NOTIFICATION_PROCEEDING:
    case EXOSIP_CALL_MESSAGE_PROCEEDING:
    case EXOSIP_SUBSCRIPTION_PROCEEDING:
    case EXOSIP_CALL_PROCEEDING:
        return result + "/proceed";
    case EXOSIP_CALL_RINGING:
        return result + "/ring";
    case EXOSIP_MESSAGE_ANSWERED:
    case EXOSIP_CALL_ANSWERED:
    case EXOSIP_CALL_MESSAGE_ANSWERED:
    case EXOSIP_SUBSCRIPTION_ANSWERED:
    case EXOSIP_NOTIFICATION_ANSWERED:
        return result + "/answer";
    case EXOSIP_SUBSCRIPTION_REDIRECTED:
    case EXOSIP_NOTIFICATION_REDIRECTED:
    case EXOSIP_CALL_MESSAGE_REDIRECTED:
    case EXOSIP_CALL_REDIRECTED:
    case EXOSIP_MESSAGE_REDIRECTED:
        return result + "/redirect";
    case EXOSIP_REGISTRATION_FAILURE:
        return result + "/noreg";
    case EXOSIP_SUBSCRIPTION_REQUESTFAILURE:
    case EXOSIP_NOTIFICATION_REQUESTFAILURE:
    case EXOSIP_CALL_REQUESTFAILURE:
    case EXOSIP_CALL_MESSAGE_REQUESTFAILURE:
    case EXOSIP_MESSAGE_REQUESTFAILURE:
        return result + "/failed";
    case EXOSIP_SUBSCRIPTION_SERVERFAILURE:
    case EXOSIP_NOTIFICATION_SERVERFAILURE:
    case EXOSIP_CALL_SERVERFAILURE:
    case EXOSIP_CALL_MESSAGE_SERVERFAILURE:
    case EXOSIP_MESSAGE_SERVERFAILURE:
        return result + "/server";
    case EXOSIP_SUBSCRIPTION_GLOBALFAILURE:
    case EXOSIP_NOTIFICATION_GLOBALFAILURE:
    case EXOSIP_CALL_GLOBALFAILURE:
    case EXOSIP_CALL_MESSAGE_GLOBALFAILURE:
    case EXOSIP_MESSAGE_GLOBALFAILURE:
        return result + "/global";
    case EXOSIP_CALL_ACK:
        return result + "/ack";
    case EXOSIP_CALL_CLOSED:
    case EXOSIP_CALL_RELEASED:
        return result + "/bye";
    case EXOSIP_CALL_CANCELLED:
        return result + "/cancel";
    case EXOSIP_MESSAGE_NEW:
    case EXOSIP_CALL_MESSAGE_NEW:
    case EXOSIP_IN_SUBSCRIPTION_NEW:
        return result + "/new";
    case EXOSIP_SUBSCRIPTION_NOTIFY:
        return result + "/notify";
    default:
        break;
    }
    return "unknown";
}
