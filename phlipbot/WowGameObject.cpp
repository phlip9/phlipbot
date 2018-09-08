#include "WowGameObject.hpp"

#include <iomanip>
#include <stdint.h>

#include "memory.hpp"

using std::ostream;
using std::string;

using phlipbot::memory::ReadCStr;
using phlipbot::memory::ReadRaw;
using phlipbot::offsets::Descriptor;

namespace phlipbot
{
string WowGameObject::GetName() const
{
  uintptr_t name_cache_ptr =
    GetDescriptor<uintptr_t>(Descriptor::GameObjNamePtr);
  if (!name_cache_ptr) return "";

  uintptr_t name_ptr = ReadRaw<uintptr_t>(name_cache_ptr + 0x8);
  if (!name_ptr) return "";

  return ReadCStr(name_ptr, 0x40);
}

ObjectType WowGameObject::GetObjectType() const { return ObjectType::GAMEOBJ; }

void WowGameObject::PrintToStream(ostream& os) const
{
  vec3 pos = GetPosition();

  os << std::hex << std::setfill('0');
  os << "{ type: WowGameObject";
  os << ", guid: 0x" << std::setw(16) << guid;
  os << ", base_ptr: 0x" << std::setw(8) << base_ptr;
  os << ", name: " << GetName();
  // os << ", created_by: 0x" << std::setw(16) << GetCreatedByGuid();
  os << ", position: { " << pos.x << ", " << pos.y << ", " << pos.z << " } ";
  os << " }";
}
}
