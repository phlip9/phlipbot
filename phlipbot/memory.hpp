#pragma once

#include <stdint.h>
#include <string>

#include <hadesmem/detail/assert.hpp>

namespace phlipbot
{
namespace memory
{
template <typename T>
inline T ReadRaw(uintptr_t const offset)
{
  T t;
  std::memcpy(&t, reinterpret_cast<void*>(offset), sizeof(t));
  return t;
}

template <typename T>
inline T& ReadRaw(uintptr_t const offset, T& t)
{
  std::memcpy(t, reinterpret_cast<void*>(offset), sizeof(t));
  return t;
}

inline std::string ReadCStr(uintptr_t const offset, size_t const max_size)
{
  char const* str_begin = reinterpret_cast<char const*>(offset);
  char const* str_end =
    reinterpret_cast<char const*>(std::memchr(str_begin, '\0', max_size));
  HADESMEM_DETAIL_ASSERT(str_end != nullptr);

  size_t const len = static_cast<size_t>(str_end - str_begin);
  return std::string{str_begin, len};
}

template <typename T>
inline void WriteRaw(uintptr_t const offset, T const& t)
{
  auto const target_ptr = reinterpret_cast<T*>(offset);
  *target_ptr = t;
}
}
}