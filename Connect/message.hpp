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

#ifndef MESSAGE_HPP_
#define MESSAGE_HPP_

#include "../Common/compiler.hpp"
#include "../Common/util.hpp"
#include "../Common/contact.hpp"
#include "../Common/crypto.hpp"

#include <QSslCertificate>

#define X_ROSTER        "X-ROSTER"
#define X_PROFILE       "X-PROFILE"
#define X_DEVLIST       "X-DEVLIST"
#define X_ADMIN         "X-ADMIN"
#define X_DROP          "X-DROP"
#define X_DEVKILL       "X-DEVKILL"
#define X_PENDING       "X-PENDING"
#define A_PENDING       "A-PENDING"
#define X_TOPIC         "X-TOPIC"
#define X_AUTHORIZE     "X-AUTHORIZE"
#define X_DEAUTHORIZE   "X-DEAUTHORIZE"
#define X_MEMBERSHIP    "X-MEMBERSHIP"
#define X_COVERAGE      "X-COVERAGE"
#define X_FORWARDING    "X-FORWARDING"

#define MSG_IS_ROSTER(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_ROSTER))

#define MSG_IS_DEVLIST(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_DEVLIST))

#define MSG_IS_DEVKILL(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_DEVKILL))

#define MSG_IS_PENDING(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_PENDING))

#define MSG_IS_PROFILE(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_PROFILE))

#define MSG_IS_AUTHORIZE(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_AUTHORIZE))

#define MSG_IS_DEAUTHORIZE(msg)   (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_DEAUTHORIZE))

#define MSG_IS_MEMBERSHIP(msg)  (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_MEMBERSHIP))

#define MSG_IS_TOPIC(msg)  (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_TOPIC))

#define MSG_IS_COVERAGE(msg) (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_COVERAGE))

#define MSG_IS_FORWARDING(msg) (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_FORWARDING))

#define MSG_IS_ADMIN(msg) (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_ADMIN))

#define MSG_IS_DROP(msg) (MSG_IS_REQUEST(msg) && \
    0==strcmp((msg)->sip_method, X_DROP))

class Message
{
protected:
    void add_authentication(osip_message_t *msg, const QVariantHash& creds) {
        add_authorization(msg, creds, true);
    }

    static bool sign_body(osip_message_t *msg, const QByteArray& prikey);
    static bool verify_body(osip_message_t *msg, const QByteArray& pubkey);
    static void add_authorization(osip_message_t *msg, const QVariantHash& creds, bool authenticate = false);
    static void dump(osip_message_t *msg);
    const char *toStr(eXosip_event_type ev);
};

class Locker final
{
public:
    explicit inline Locker(eXosip_t *ctx) :
    context(ctx) {
        Q_ASSERT(context != nullptr);
        eXosip_lock(context);
    }

    inline ~Locker() {
        eXosip_unlock(context);
    }

private:
    eXosip_t *context;
};

/*!
 * Common sip messaging manipulation functions.  These really only depend
 * on and front-end existing osip functions with new convenient functions.
 * \file message.hpp
 */

#endif
