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

#ifndef INLINE_HPP_
#define INLINE_HPP_

#include "compiler.hpp"

namespace Util {
    template<typename T>
    T clampRange(const T& value, const T& min, const T& max) {
        if(value < min)
            return min;

        if(value > max)
            return max;

        return value;
    }

    template<typename T>
    bool inRange(const T& value, const T& min, const T& max) {
        return !(value < min || value > max);
    }
}

/*!
 * Generic inline template functions and classes.
 * \file inline.hpp
 */

/*!
 * \fn Util::clampRange(const T& value, const T& min, const T& max)
 * Clamp the range of the value so if <min, it becomes min, and >max then
 * it becomes max.
 * \param value The value to be clamped.
 * \param min The minimum value permitted.
 * \param max The maximum value permitted.
 * \return value within min/max range.
 *
 * \fn Util::inRange(const T& value, const T& min, const T& max)
 * Determine if value is within clamped range.
 * \param value The value to test.
 * \param min The minimum value permitted.
 * \param max The maximum value allowed.
 * \return true if value >= min && value <= max.
 */

#endif
