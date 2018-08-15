#pragma once

#include "wow_constants.hpp"

#include "WowUnit.hpp"

namespace phlipbot
{
struct WowPlayer : WowUnit {
public:
  explicit WowPlayer(types::Guid const guid, uintptr_t const base_ptr)
    : WowUnit(guid, base_ptr)
  {
  }
  WowPlayer(const WowPlayer& obj) = default;
  virtual ~WowPlayer() = default;

  virtual std::string GetName() const override;

  virtual phlipbot::types::ObjectType GetObjectType() const override;

  virtual void PrintToStream(std::ostream& os) const override;
};
}
