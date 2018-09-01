#pragma once

#include "WowObject.hpp"

namespace phlipbot
{
struct WowGameObject : WowObject {
public:
  explicit WowGameObject(Guid const guid, uintptr_t const base_ptr)
    : WowObject(guid, base_ptr)
  {
  }
  WowGameObject(const WowGameObject& obj) = default;
  virtual ~WowGameObject() = default;

  inline Vec3 GetPosition() const
  {
    return GetDescriptor<Vec3>(offsets::Descriptor::GameObjPos);
  }

  // TODO(phlip9): don't think this is correct
  /*
  inline types::Guid WowGameObject::GetCreatedByGuid() const
  {
    return GetDescriptor<types::Guid>(
      offsets::Descriptor::GameObjectCreatedByGuid);
  }
  */

  virtual std::string GetName() const override;

  virtual ObjectType GetObjectType() const override;

  virtual void PrintToStream(std::ostream& os) const override;
};
}
