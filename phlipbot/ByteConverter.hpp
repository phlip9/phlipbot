#pragma once

/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2009-2011 MaNGOSZero <https://github.com/mangos/zero>
 * Copyright (C) 2011-2016 Nostalrius <https://nostalrius.org>
 * Copyright (C) 2016-2017 Elysium Project <https://github.com/elysium-project>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/** ByteConverter reverse your byte order.  This is use
    for cross platform where they have different endians.
 */

#include <algorithm>
#include <stdint.h>

// Compile-time test for endianness
// (though compiling on windows, we're always going to be little endian)
// TODO(phlip9): probably better solution
// http://esr.ibiblio.org/?p=5095
// https://godbolt.org/z/qMXiAp
#define IS_BIG_ENDIAN (*(uint32_t*)"\x00\x00\x00\xFF" < 0x00000100)

namespace ByteConverter
{
template <size_t T>
inline void convert(char* val)
{
  std::swap(*val, *(val + T - 1));
  convert<T - 2>(val + 1);
}

template <>
inline void convert<0>(char*)
{
}
template <>
inline void convert<1>(char*)
{
} // ignore central byte

template <typename T>
inline void apply(T* val)
{
  convert<sizeof(T)>(reinterpret_cast<char*>(val));
}

template <typename T>
inline void to_le(T& val)
{
  if (IS_BIG_ENDIAN) {
    apply<T>(&val);
  }
}
template <typename T>
inline void to_be(T& val)
{
  if (!IS_BIG_ENDIAN) {
    apply<T>(&val);
  }
}

template <typename T>
void to_le(T*); // will generate link error
template <typename T>
void to_be(T*); // will generate link error

inline void to_le(uint8_t&) {}
inline void to_le(int8_t&) {}
inline void to_be(uint8_t&) {}
inline void to_be(int8_t&) {}
}
