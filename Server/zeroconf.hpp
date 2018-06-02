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

#ifndef ZEROCONF_HPP_
#define ZEROCONF_HPP_

#include "server.hpp"
#include <QThread>

class Zeroconfig final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Zeroconfig)

public:
    Zeroconfig(Server *server, quint16 port);

    static bool enabled();

public slots:
    void onStartup();
    void onShutdown();
    
};

#endif
