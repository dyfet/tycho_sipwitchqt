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

#ifndef	COMPILER_HPP_
#define COMPILER_HPP_

#include "config.hpp"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-result"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#if __cplusplus <= 199711L && !defined(_MSC_VER)
#error C++11 compliant compiler required
#endif

#ifdef _MSC_VER
#pragma warning(disable: 4068)
#pragma warning(disable: 26451)
#pragma warning(disable: 26444)
#pragma warning(disable: 26495)
#if _MSC_VER < 1900
#error "VS >= 2015 or later required"
#endif

#if !defined(_MT) && !defined(__MT__)
#error "multi-threaded build required"
#endif
#endif

#ifdef QT_NO_DEBUG
#define FOR_DEBUG(x)
#define FOR_RELEASE(x)    {x;}
#else
#define FOR_DEBUG(x)      {x;}
#define FOR_RELEASE(x)
#endif

#ifndef NORETURN
#define NORETURN [[noreturn]]
#endif

/*!
 * Provides pragmas and macros for use in every compilation unit.
 * \file compiler.hpp
 */

/*!
 * \def FOR_DEBUG(block)
 * Defines a block of code only built for debug builds.
 */

/*!
 * \def FOR_RELEASE(block)
 * Defines a block of code only built in release builds.
 */

#endif
