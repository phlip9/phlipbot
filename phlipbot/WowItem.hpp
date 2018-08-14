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

  inline uint32_t GetItemId()
  {
    return GetDescriptor<uint32_t>(phlipbot::offsets::Descriptors::ItemId);
  }

  inline uint32_t GetStackCount()
  {
    return GetDescriptor<uint32_t>(
      phlipbot::offsets::Descriptors::ItemStackCount);
  }

  inline uint32_t GetDurability()
  {
    return GetDescriptor<uint32_t>(
      phlipbot::offsets::Descriptors::ItemDurability);
  }

  inline uint32_t GetMaxDurability()
  {
    return GetDescriptor<uint32_t>(
      phlipbot::offsets::Descriptors::ItemMaxDurability);
  }

  inline float GetDurabilityPercent()
  {
    float const durability = static_cast<float>(GetDurability());
    float const max_durability = static_cast<float>(GetMaxDurability());
    return (durability / max_durability) * 100;
  }

  virtual std::string GetName() override;

  virtual phlipbot::types::ObjectType GetObjectType() const override;

  phlipbot::types::ItemQuality GetQuality();
};
}
