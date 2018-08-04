#include "WowGameObject.hpp"

#include <stdint.h>

#include "wow_constants.hpp"
#include "memory.hpp"
#include "WowObject.hpp"

using namespace phlipbot::types;
using namespace phlipbot::offsets;

namespace phlipbot
{
std::string WowGameObject::GetName()
{
  uintptr_t ptr1 = GetRelative<uintptr_t>(0x214);

  if (!ptr1) return "";

  uintptr_t ptr2 = phlipbot::memory::ReadRaw<uintptr_t>(ptr1 + 0x8);

  if (!ptr2) return "";

  return phlipbot::memory::ReadCStr(ptr2, 0x40);
}
}
