#pragma once

#include <string>

#include "wow_constants.hpp"

#include "WowObject.hpp"

namespace phlipbot
{
struct WowItem : WowObject {
public:
  explicit WowItem(types::Guid const guid, uintptr_t const base_ptr)
    : WowObject(guid, base_ptr)
  {
  }
  WowItem(const WowItem& obj) = default;
  virtual ~WowItem() = default;

  virtual uint32_t GetItemId() const;

  // TODO(phlip9): debug stack count and durability

  inline uint32_t GetStackCount() const
  {
    return GetDescriptor<uint32_t>(
      phlipbot::offsets::Descriptors::ItemStackCount);
  }

  inline uint32_t GetDurability() const
  {
    return GetDescriptor<uint32_t>(
      phlipbot::offsets::Descriptors::ItemDurability);
  }

  inline uint32_t GetMaxDurability() const
  {
    return GetDescriptor<uint32_t>(
      phlipbot::offsets::Descriptors::ItemMaxDurability);
  }

  inline float GetDurabilityPercent() const
  {
    float const durability = static_cast<float>(GetDurability());
    float const max_durability = static_cast<float>(GetMaxDurability());
    return (durability / max_durability) * 100;
  }

  virtual std::string GetName() const override;

  virtual phlipbot::types::ObjectType GetObjectType() const override;

  virtual void PrintToStream(std::ostream& os) const override;

  phlipbot::types::ItemQuality GetQuality() const;
};
}
