#pragma once

#include "wow_constants.hpp"

#include "WowItem.hpp"

namespace phlipbot
{
struct WowContainer : WowItem {
public:
  explicit WowContainer(types::Guid const guid, uintptr_t const base_ptr)
    : WowItem(guid, base_ptr)
  {
  }
  WowContainer(const WowContainer& obj) = default;
  virtual ~WowContainer() = default;

  virtual phlipbot::types::ObjectType GetObjectType() const override;
};
}
