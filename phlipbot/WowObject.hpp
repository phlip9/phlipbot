#pragma once

#include <iostream>
#include <stdint.h>
#include <string>

#include "memory.hpp"
#include "wow_constants.hpp"

namespace phlipbot
{
struct WowObject {
  inline explicit WowObject(Guid const guid_, uintptr_t const base_ptr_)
    : guid{guid_}, base_ptr{base_ptr_}
  {
  }
  WowObject(const WowObject& obj) = default;
  virtual ~WowObject() = default;

  template <typename T>
  inline T GetDescriptor(offsets::Descriptors const desc_offset) const
  {
    uintptr_t const target = base_ptr +
                             offsets::ObjectManagerOffsets::DescriptorOffset +
                             static_cast<ptrdiff_t>(desc_offset);

    return memory::ReadRaw<T>(target);
  }

  template <typename T>
  inline void SetDescriptor(offsets::Descriptors const desc_offset, T const& t)
  {
    uintptr_t const target = base_ptr +
                             offsets::ObjectManagerOffsets::DescriptorOffset +
                             static_cast<ptrdiff_t>(desc_offset);

    memory::WriteRaw<T>(target, t);
  }

  template <typename T>
  inline T GetRelative(ptrdiff_t const offset) const
  {
    return memory::ReadRaw<T>(base_ptr + offset);
  }

  template <typename T>
  inline void SetRelative(ptrdiff_t const offset, T const& t)
  {
    memory::WriteRaw<T>(base_ptr + offset, t);
  }

  virtual std::string GetName() const;
  virtual ObjectType GetObjectType() const;

  virtual void PrintToStream(std::ostream& os) const;
  std::string ToString() const;
  friend std::ostream& operator<<(std::ostream& os, WowObject const* obj);

  Guid const guid;
  uintptr_t const base_ptr;
};
}