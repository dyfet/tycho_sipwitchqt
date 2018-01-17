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
#include <QDebug>

#ifndef SESSION_EXPIRES
#define SESSION_EXPIRES "session-expires"
#endif

Event::Data::Data() :
expires(-1), status(0), hops(0), natted(false), local(false), associated(false), context(nullptr), event(nullptr), message(nullptr), authorization(nullptr)
{
}

Event::Data::Data(eXosip_event_t *evt, Context *ctx) :
expires(-1), status(0), hops(0), natted(false), local(false), associated(false), context(ctx), event(evt), message(nullptr), authorization(nullptr)
{
    // start time of event creation
    elapsed.start();

    // ignore constructor parser if empty event;
    if(!evt) {
        context = nullptr;
        return;
    }

    // start of event decompose by event type...
    switch(evt->type) {
    case EXOSIP_MESSAGE_ANSWERED:
    case EXOSIP_REGISTRATION_SUCCESS:       // provider succeeded
    case EXOSIP_REGISTRATION_FAILURE:       // provider failed
        status = evt->response->status_code;
        reason = evt->response->reason_phrase;
        parseMessage(evt->response);
        break;
    case EXOSIP_MESSAGE_NEW:
    case EXOSIP_CALL_INVITE:
        method = UString(evt->request->sip_method).toUpper();
        if(osip_message_get_authorization(evt->request, 0, &authorization) != 0 || !authorization->username || !authorization->response)
            authorization = nullptr;
        parseMessage(evt->request);
        if(evt->request->req_uri && evt->request->req_uri->host) {
            request = evt->request->req_uri;
            target = Contact(ctx->hostname(), ctx->port(), request.user());
            local = ctx->isLocal(request.host());
        }
        break;
    default:
        break;
    }

    // parse out authorization for later use
    if(authorization && authorization->username)
        userid = UString(authorization->username).unquote();
    if(authorization && authorization->response)
        digest = UString(authorization->response).unquote();
    if(authorization && authorization->nonce)
        nonce = UString(authorization->nonce).unquote();
    if(authorization && authorization->realm)
        realm = UString(authorization->realm).unquote();
    if(authorization && authorization->algorithm)
        algorithm = UString(authorization->algorithm).unquote();
}

Event::Data::~Data()
{
    if(event) {
        qDebug().nospace() << "~Event(" << event->type << ",cid=" << event->cid << ",did=" << event->did << ",ctx=" << context->objectName() << ",source=" << source.toString() << ")";
        eXosip_event_free(event);
        event = nullptr;
    }
}

void Event::Data::parseMessage(osip_message_t *msg)
{
    const osip_list_t& clist = msg->contacts;
    const osip_list_t& vlist = msg->vias;
    const osip_list_t& rlist = msg->record_routes;
    const osip_list_t& alist = msg->allows;
    int pos;
    Contact nat;

    message = msg;
    if(!msg)
        return;

    pos = 0;
    while(osip_list_eol(&vlist, pos) == 0) {
        auto via = static_cast<osip_via_t *>(osip_list_get(&vlist, pos++));
        ++hops;
        if(via->host) {
            const char *addr = via->host;
            quint16 port = context->defaultPort(), rport = 0;
            if(via->port)
                port = Util::portNumber(via->port);
            osip_uri_param_t *param = nullptr;
            osip_via_param_get_byname(via, const_cast<char *>("rport"), &param);
            if(param && param->gvalue)
                rport = Util::portNumber(param->gvalue);
            if(via->port)
                port = Util::portNumber(via->port);
            source = Contact(addr, port);

            // top nat only
            if(!nat && rport) {
                param = nullptr;
                osip_via_param_get_byname(via, const_cast<char *>("received"), &param);
                if(param && param->gvalue)
                    nat = Contact(param->gvalue, rport);
            }
        }
    }

    // nat overrides last via contact source address
    if(nat) {
        natted = true;
        source = nat;
    }

    osip_header_t *header;

    header = nullptr;
    osip_message_header_get_byname(msg, USER_AGENT, 0, &header);
    if(header && header->hvalue)
        agent = header->hvalue;

    header = nullptr;
    osip_message_header_get_byname(msg, SESSION_EXPIRES, 0, &header);
    if(header && header->hvalue)
        expires = Util::portNumber(header->hvalue);

    header = nullptr;
    osip_message_header_get_byname(msg, "x-label", 0, &header);
    if(header && header->hvalue)
        label = UString(header->hvalue).toLower();
    else
        label = "NONE";

    header = nullptr;
    osip_message_header_get_byname(msg, "subject", 0, &header);
    if(header && header->hvalue)
        subject = header->hvalue;


    pos = 0;
    while(osip_list_eol(&alist, pos) == 0) {
        auto allow = static_cast<osip_allow_t *>(osip_list_get(&alist, pos++));
        if(allow && allow->value)
            allows << UString(allow->value).toLower();
    }

    pos = 0;
    while(osip_list_eol(&clist, pos) == 0) {
        auto contact = static_cast<osip_contact_t *>(osip_list_get(&clist, pos++));
        if(contact && contact->url && contact->url->host)
            contacts << Contact(contact);
    }

    pos = 0;
    while(osip_list_eol(&rlist, pos) == 0) {
        auto recroute = static_cast<osip_record_route_t*>(osip_list_get(&rlist, pos++));
        if(recroute->url && recroute->url->host) {
            routes << Contact(recroute->url);
        }
    }

    if(routes.count() > 0)
        record = true;
    else {
        osip_route_t *route;
        pos = 0;
        const osip_list_t& list = msg->routes;
        while(osip_list_eol(&list, pos) == 0) {
            route = static_cast<osip_route_t *>(osip_list_get(&list, pos++));
            if(route->url && route->url->host) {
                routes << Contact(route->url);
            }
        }
    }

    if(msg->from)
        from = msg->from->url;
    if(msg->to)
        to = msg->to->url;

    if(msg->content_type && msg->content_type->type) {
        content = msg->content_type->type;
        if(msg->content_type->subtype)
            content += UString("/") + msg->content_type->subtype;
    }

    osip_body_t *data = nullptr;
    osip_message_get_body(msg, 0, &data);
    if(data && data->length)
        body = QByteArray(data->body, static_cast<int>(data->length));
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

const UString Event::text() const
{
    UString result;

    if(!d->message)
        return result;

    char *data = nullptr;
    size_t len;
    osip_message_to_str(d->message, &data, &len);
    if(data) {
        data[len] = 0;
        result = data;
        osip_free(data);
    }
    return result;
}

const UString Event::uriContext(const UString& username) const
{
    if(!d->context)
        return "";

    return d->context->uriFrom(username);
}

// used for events that support only one contact object...
const Contact Event::contact() const
{
    if(d->contacts.size() != 1)
        return Contact();
    return d->contacts[0];
}

const UString Event::protocol() const
{
    Q_ASSERT(d->context != nullptr);
    return d->context->type();
}

inline const UString Event::uri(const Contact& addr) const {
    Q_ASSERT(d->context != nullptr);
    return d->context->uriTo(addr);
}

QDebug operator<<(QDebug dbg, const Event& ev)
{
    if(ev) {
        if(ev.status())
            dbg.nospace() << "Event(" << ev.toString() << ",cid=" << ev.cid() << ",did=" << ev.did() << ",ctx=" << ev.context()->objectName() << ",source=" << ev.source().toString() << ",status=" << ev.status() << ")";
        else
            dbg.nospace() << "Event(" << ev.toString() << ",cid=" << ev.cid() << ",did=" << ev.did() << ",ctx=" << ev.context()->objectName() << ",source=" << ev.source().toString() << ",target=" << ev.target().toString() << ")";
    }
    else
        dbg.nospace() << "Event(timeout)";
    return dbg.maybeSpace();
}

const UString Event::toString() const
{
    if(!d->event)
        return "none";

    UString result = UString::number(d->event->type);
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

