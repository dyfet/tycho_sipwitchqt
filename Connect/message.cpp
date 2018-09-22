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

#include "../Common/crypto.hpp"
#include "message.hpp"

bool Message::verify_body(osip_message_t *msg, const QByteArray& pubkey)
{
    if(pubkey.count() < 1)
        return false;

    osip_body_t *body = nullptr;
    osip_header_t *header = nullptr;
    osip_message_header_get_byname(msg, "x-signature", 0, &header);
    if(!header || !header->hvalue)
        return false;
    auto sig = QByteArray::fromHex(header->hvalue);
    osip_message_get_body(msg, 0, &body);
    if(body && body->body && body->length > 0) {
        QByteArray data(body->body, static_cast<int>(body->length));
        return Crypto::verify(pubkey, data, sig);
    }
    return false;
}

bool Message::sign_body(osip_message_t *msg, const QByteArray& prikey)
{
    if(prikey.count() < 1)
        return false;

    osip_body_t *body = nullptr;
    osip_message_get_body(msg, 0, &body);
    if(body && body->body && body->length > 0) {
        QByteArray data(body->body, static_cast<int>(body->length));
        auto signature = Crypto::sign(prikey, data).toHex();
        osip_message_set_header(msg, "x-signature", signature);
        return true;
    }
    return false;
}

void Message::add_authorization(osip_message_t *msg, const QVariantHash& creds, bool authenticate)
{
    UString user = creds["user"].toString();
    UString secret = creds["secret"].toString();
    UString realm = creds["realm"].toString();
    UString algo = creds["algorithm"].toString();
    UString nonce = creds["nonce"].toString();

    auto digest = Crypto::digests[algo];
    char *uri = nullptr;
    osip_uri_to_str(msg->req_uri, &uri);

    UString method = msg->sip_method;
    UString ha1 = QCryptographicHash::hash(user + ":" + realm + ":" + secret, digest).toHex().toLower();
    UString ha2 = QCryptographicHash::hash(method + ":" + uri, digest).toHex().toLower();
    UString response = QCryptographicHash::hash(ha1 + ":" + nonce + ":" + ha2, digest).toHex().toLower();
    UString auth = "Digest username=\"" + user +
        "\", realm=\"" + realm +
        "\", uri=\"" + uri +
        "\", response=\"" + response +
        "\", nonce=\"" + nonce +
        "\", algorithm=\"" + algo + "\"";

    if(authenticate)
        osip_message_set_header(msg, WWW_AUTHENTICATE, auth);
    else
        osip_message_set_header(msg, AUTHORIZATION, auth);
}

void Message::dump(osip_message_t *msg)
{
    if(!msg)
        return;

    char *data = nullptr;
    size_t len;
    osip_message_to_str(msg, &data, &len);
    if(data) {
        data[len] = 0;
        qDebug() << "MSG" << data;
        osip_free(data);
    }
}

const char *Message::toStr(eXosip_event_type ev)
{
    switch(ev) {
    case EXOSIP_REGISTRATION_SUCCESS:
        return "register";
    case EXOSIP_CALL_INVITE:
        return "invite";
    case EXOSIP_CALL_REINVITE:
        return "reinvite";
    case EXOSIP_CALL_NOANSWER:
    case EXOSIP_SUBSCRIPTION_NOANSWER:
    case EXOSIP_NOTIFICATION_NOANSWER:
        return "noanswer";
    case EXOSIP_MESSAGE_PROCEEDING:
    case EXOSIP_NOTIFICATION_PROCEEDING:
    case EXOSIP_CALL_MESSAGE_PROCEEDING:
    case EXOSIP_SUBSCRIPTION_PROCEEDING:
    case EXOSIP_CALL_PROCEEDING:
        return "proceed";
    case EXOSIP_CALL_RINGING:
        return "ring";
    case EXOSIP_MESSAGE_ANSWERED:
    case EXOSIP_CALL_ANSWERED:
    case EXOSIP_CALL_MESSAGE_ANSWERED:
    case EXOSIP_SUBSCRIPTION_ANSWERED:
    case EXOSIP_NOTIFICATION_ANSWERED:
        return "answer";
    case EXOSIP_SUBSCRIPTION_REDIRECTED:
    case EXOSIP_NOTIFICATION_REDIRECTED:
    case EXOSIP_CALL_MESSAGE_REDIRECTED:
    case EXOSIP_CALL_REDIRECTED:
    case EXOSIP_MESSAGE_REDIRECTED:
        return "redirect";
    case EXOSIP_REGISTRATION_FAILURE:
        return "noreg";
    case EXOSIP_SUBSCRIPTION_REQUESTFAILURE:
    case EXOSIP_NOTIFICATION_REQUESTFAILURE:
    case EXOSIP_CALL_REQUESTFAILURE:
    case EXOSIP_CALL_MESSAGE_REQUESTFAILURE:
    case EXOSIP_MESSAGE_REQUESTFAILURE:
        return "failed";
    case EXOSIP_SUBSCRIPTION_SERVERFAILURE:
    case EXOSIP_NOTIFICATION_SERVERFAILURE:
    case EXOSIP_CALL_SERVERFAILURE:
    case EXOSIP_CALL_MESSAGE_SERVERFAILURE:
    case EXOSIP_MESSAGE_SERVERFAILURE:
        return "server";
    case EXOSIP_SUBSCRIPTION_GLOBALFAILURE:
    case EXOSIP_NOTIFICATION_GLOBALFAILURE:
    case EXOSIP_CALL_GLOBALFAILURE:
    case EXOSIP_CALL_MESSAGE_GLOBALFAILURE:
    case EXOSIP_MESSAGE_GLOBALFAILURE:
        return "global";
    case EXOSIP_CALL_ACK:
        return "ack";
    case EXOSIP_CALL_CLOSED:
    case EXOSIP_CALL_RELEASED:
        return "bye";
    case EXOSIP_CALL_CANCELLED:
        return "cancel";
    case EXOSIP_MESSAGE_NEW:
    case EXOSIP_CALL_MESSAGE_NEW:
    case EXOSIP_IN_SUBSCRIPTION_NEW:
        return "new";
    case EXOSIP_SUBSCRIPTION_NOTIFY:
        return "notify";
    default:
        break;
    }
    return "unknown";
}
