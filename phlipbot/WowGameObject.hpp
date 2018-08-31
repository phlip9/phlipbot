#pragma once

#include <iostream>
#include <string>

#include "WowObject.hpp"
#include "wow_constants.hpp"

namespace phlipbot
{
struct WowGameObject : WowObject {
public:
  explicit WowGameObject(types::Guid const guid, uintptr_t const base_ptr)
    : WowObject(guid, base_ptr)
  {
  }
  WowGameObject(const WowGameObject& obj) = default;
  virtual ~WowGameObject() = default;

  inline types::Vec3 WowGameObject::GetPosition() const
  {
    return GetDescriptor<types::Vec3>(offsets::Descriptors::GameObjPos);
  }

  // TODO(phlip9): don't think this is correct
  /*
  inline types::Guid WowGameObject::GetCreatedByGuid() const
  {
    return GetDescriptor<types::Guid>(
      offsets::Descriptors::GameObjectCreatedByGuid);
  }
  */

  virtual std::string GetName() const override;

  virtual phlipbot::types::ObjectType GetObjectType() const override;

  virtual void PrintToStream(std::ostream& os) const override;
};
}
