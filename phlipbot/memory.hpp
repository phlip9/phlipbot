#pragma once

#include <string>
#include <stdint.h>

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

inline std::string ReadCStr(uintptr_t const offset, size_t const max_size)
{
  char* const buf = new char[max_size + 1];
  std::strncpy(buf, reinterpret_cast<char*>(offset), max_size);
  std::string result(buf);
  delete[] buf;
  return result;
}

template <typename T>
inline void WriteRaw(uintptr_t const offset, T const& t)
{
  auto const target_ptr = reinterpret_cast<T*>(offset);
  *target_ptr = t;
}
}
}