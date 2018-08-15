#include "WowPlayer.hpp"

#include <iomanip>
#include <iostream>
#include <stdint.h>
#include <string>

#include "memory.hpp"
#include "wow_constants.hpp"

using namespace phlipbot::types;
using namespace phlipbot::offsets::Data;

namespace phlipbot
{
std::string WowPlayer::GetName() const
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


void WowPlayer::PrintToStream(std::ostream& os) const
{
  XYZ pos = GetPosition();

  os << std::hex << std::setfill('0');
  os << "{ type: WowPlayer";
  os << ", guid: 0x" << std::setw(16) << guid;
  os << ", base_ptr: 0x" << std::setw(8) << base_ptr;
  os << ", name: " << GetName();
  os << ", position: { " << pos.X << ", " << pos.Y << ", " << pos.Z << " } ";
  os << std::dec;
  os << ", npc_id: " << GetNpcId();
  os << ", faction_id: " << GetFactionId();
  os << ", health: " << GetHealthPercent() << "%";
  os << ", mana: " << GetManaPercent() << "%";
  os << ", rage: " << GetRage();
  os << std::hex;
  os << ", target: " << std::setw(16) << GetTargetGuid();
  os << ", movement_flags: " << std::setw(8) << GetMovementFlags();
  os << ", dynamic_flags: " << std::setw(8) << GetDynamicFlags();
  os << " }";
}
}