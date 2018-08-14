#include "WowPlayer.hpp"

#include <stdint.h>
#include <string>

#include "memory.hpp"
#include "wow_constants.hpp"

using namespace phlipbot::types;
using namespace phlipbot::offsets::Data;

namespace phlipbot
{
std::string WowPlayer::GetName()
{
  uintptr_t const cache_ptr = offsets::Data::DBCache__NameCache;

  uintptr_t entry_ptr = cache_ptr + 0x8;
  Guid next_guid = memory::ReadRaw<Guid>(entry_ptr + 0xC);

  while (next_guid != 0) {
    if (next_guid == guid) {
      return memory::ReadCStr(entry_ptr + 0x14, 0x40);
    }

    entry_ptr = memory::ReadRaw<uintptr_t>(entry_ptr);
    next_guid = memory::ReadRaw<Guid>(entry_ptr + 0xC);
  }

  return "";
}

ObjectType WowPlayer::GetObjectType() const { return ObjectType::PLAYER; }
}