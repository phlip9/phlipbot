#pragma once

#include <stdint.h>

#include "memory.hpp"
#include "wow_constants.hpp"

namespace phlipbot
{
struct WowObject {
public:
  inline explicit WowObject(phlipbot::types::Guid const guid_,
                            uintptr_t const base_ptr_)
    : guid{guid_}, base_ptr{base_ptr_}
  {
  }
  WowObject(const WowObject& obj) = default;
  virtual ~WowObject() = default;

  template <typename T>
  inline T GetDescriptor(phlipbot::offsets::Descriptors const desc_offset)
  {
    uintptr_t const target =
      base_ptr + phlipbot::offsets::ObjectManagerOffsets::DescriptorOffset +
      static_cast<ptrdiff_t>(desc_offset);

    return phlipbot::memory::ReadRaw<T>(target);
  }

  template <typename T>
  inline void
  SetDescriptor(phlipbot::offsets::Descriptors const desc_offset, T const& t)
  {
    uintptr_t const target =
      base_ptr + phlipbot::offsets::ObjectManagerOffsets::DescriptorOffset +
      static_cast<ptrdiff_t>(desc_offset);

    phlipbot::memory::WriteRaw<T>(target, t);
  }

  template <typename T>
  inline T GetRelative(ptrdiff_t const offset)
  {
    return phlipbot::memory::ReadRaw<T>(base_ptr + offset);
  }

  template <typename T>
  inline void SetRelative(ptrdiff_t const offset, T const& t)
  {
    phlipbot::memory::WriteRaw<T>(base_ptr + offset, t);
  }

  virtual std::string GetName();

  virtual phlipbot::types::ObjectType GetObjectType() const;

  // TODO(phlip9): virtual to_string and << ostream operator

  phlipbot::types::Guid const guid;
  uintptr_t const base_ptr;
};
}