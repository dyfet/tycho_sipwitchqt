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

#include "../Common/compiler.hpp"
#include "../Common/server.hpp"
#include "../Common/output.hpp"
#include "../Server/main.hpp"
#include "authorize.hpp"

Authorize *Authorize::Instance = nullptr;

Authorize::Authorize(unsigned order)
{
	Q_ASSERT(Instance == nullptr);
		Instance = this;

	if(order)
        moveToThread(Server::createThread("authorize", order));
	else
        moveToThread(Server::findThread("database"));

    timer.moveToThread(thread());
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, &Authorize::onTimeout);

	Server *server = Server::instance();
    connect(thread(), &QThread::finished, this, &QObject::deleteLater);
    connect(server, &Server::changeConfig, this, &Authorize::applyConfig);
}

Authorize::~Authorize()
{
    Instance = nullptr;
}

void Authorize::onTimeout()
{
}
