#include "WowGameObject.hpp"

#include <stdint.h>

#include "WowObject.hpp"
#include "memory.hpp"
#include "wow_constants.hpp"

using namespace phlipbot::types;
using namespace phlipbot::offsets;

namespace phlipbot
{
std::string WowGameObject::GetName()
{
  uintptr_t name_cache_ptr =
    GetDescriptor<uintptr_t>(Descriptors::GameObjNamePtr);
  if (!name_cache_ptr) return "";

  uintptr_t name_ptr =
    phlipbot::memory::ReadRaw<uintptr_t>(name_cache_ptr + 0x8);
  if (!name_ptr) return "";

  return phlipbot::memory::ReadCStr(name_ptr, 0x40);
}
}
