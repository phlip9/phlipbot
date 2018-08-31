#include "WowGameObject.hpp"

#include <iomanip>
#include <iostream>
#include <stdint.h>

#include "WowObject.hpp"
#include "memory.hpp"
#include "wow_constants.hpp"

using namespace phlipbot::types;
using namespace phlipbot::offsets;

namespace phlipbot
{
std::string WowGameObject::GetName() const
{
  uintptr_t name_cache_ptr =
    GetDescriptor<uintptr_t>(Descriptors::GameObjNamePtr);
  if (!name_cache_ptr) return "";

  uintptr_t name_ptr =
    phlipbot::memory::ReadRaw<uintptr_t>(name_cache_ptr + 0x8);
  if (!name_ptr) return "";

  return phlipbot::memory::ReadCStr(name_ptr, 0x40);
}

ObjectType WowGameObject::GetObjectType() const { return ObjectType::GAMEOBJ; }

void WowGameObject::PrintToStream(std::ostream& os) const
{
  Vec3 pos = GetPosition();

  os << std::hex << std::setfill('0');
  os << "{ type: WowGameObject";
  os << ", guid: 0x" << std::setw(16) << guid;
  os << ", base_ptr: 0x" << std::setw(8) << base_ptr;
  os << ", name: " << GetName();
  //os << ", created_by: 0x" << std::setw(16) << GetCreatedByGuid();
  os << ", position: { " << pos.X << ", " << pos.Y << ", " << pos.Z << " } ";
  os << " }";
}
}
