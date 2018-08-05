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
  virtual ~WowPlayer(){};

  virtual std::string GetName();

  phlipbot::types::ObjectType const obj_type{
    phlipbot::types::ObjectType::PLAYER};
};
}
