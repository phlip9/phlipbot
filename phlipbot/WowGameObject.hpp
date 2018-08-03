#pragma once

#include <string>

#include "wow_constants.hpp"
#include "WowObject.hpp"

namespace phlipbot
{
struct WowGameObject : WowObject
{
public:
  explicit WowGameObject(
    types::Guid const guid, uintptr_t const base_ptr,
    types::ObjectType const obj_type
  ) : WowObject(guid, base_ptr, obj_type) { }

  virtual ~WowGameObject() {};

  phlipbot::types::XYZ GetPosition();
  phlipbot::types::Guid GetCreatedByGuid();
  std::string GetName();
};
}
