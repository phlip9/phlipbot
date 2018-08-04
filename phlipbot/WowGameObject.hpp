#pragma once

#include <string>

#include "wow_constants.hpp"
#include "WowObject.hpp"

namespace phlipbot
{
struct WowGameObject : WowObject
{
public:
  explicit WowGameObject(types::Guid const guid, uintptr_t const base_ptr)
    : WowObject(guid, base_ptr)
  { }
  WowGameObject(const WowGameObject &obj) = default;
  virtual ~WowGameObject() {};

  inline types::XYZ WowGameObject::GetPosition()
  {
    return GetDescriptor<types::XYZ>(offsets::Descriptors::GameObjPos);
  }

  inline types::Guid WowGameObject::GetCreatedByGuid()
  {
    return GetDescriptor<types::Guid>(
      offsets::Descriptors::GameObjectCreatedByGuid);
  }

  virtual std::string GetName();

  phlipbot::types::ObjectType const obj_type{
    phlipbot::types::ObjectType::GAMEOBJ };
};
}
