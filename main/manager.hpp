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

#ifndef __MANAGER_HPP__
#define __MANAGER_HPP__

#include "../sip/stack.hpp"
#include "../sql/query.hpp"

class Manager : public Stack
{
    Q_DISABLE_COPY(Manager)

public:
    static void init(unsigned order);

private:
    Manager(unsigned order);
    ~Manager();

    void applyNames();

    static QString SystemPassword;
    static QString ServerHostname;
    static QString ServerMode;

private slots:
    void applyValue(const QString& id, const QVariant& value);
    void applyConfig(const QVariantHash& config);

#ifndef QT_NO_DEBUG
    void reportCounts(const QString& id, int count);
#endif
};

/*!
 * Derived sip stack management support.
 * \file manager.hpp
 */

/*!
 * \class Manager
 * \brief SIP Stack management.
 * This derives from Stack and is (will be) used to impliment SipWitchQt specific
 * behaviors, typically by overriding Stack virtuals.  It also provides
 * voip specific configuration support in conjunction with the Server.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
