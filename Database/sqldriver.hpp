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

#ifndef SQLDRIVER_HPP_
#define SQLDRIVER_HPP_

#include <QString>

namespace Util {
    const QStringList createQuery(const QString& name);
    const QStringList pragmaQuery(const QString& name);
    const QStringList preloadConfig(const QString& name);
    bool dbIsFile(const QString& name);
}

/*!
 * Some database support utility classes and functions.
 * \file sqldriver.hpp
 */

#endif
