#include "WowUnit.hpp"

#include <iomanip>
#include <iostream>
#include <stdint.h>
#include <string>
#include <vector>

#include "memory.hpp"

using namespace phlipbot::types;

namespace phlipbot
{
std::string WowUnit::GetName() const
{
  uintptr_t const name_ptr =
    GetDescriptor<uintptr_t>(offsets::Descriptors::UnitNamePtr);
  if (!name_ptr) return "";

  uintptr_t const str_ptr = memory::ReadRaw<uintptr_t>(name_ptr);
  if (!str_ptr) return "";

  // TODO(phlip9): need to reverse actual max size
  return memory::ReadCStr(str_ptr, 0x40);
}

ObjectType WowUnit::GetObjectType() const { return ObjectType::UNIT; }

std::vector<uint32_t> WowUnit::GetBuffIds() const
{
  std::vector<uint32_t> buffs;

  ptrdiff_t buff_offset =
    offsets::ObjectManagerOffsets::DescriptorOffset +
    static_cast<ptrdiff_t>(offsets::Descriptors::FirstBuff);

  ptrdiff_t const next_offset =
    static_cast<ptrdiff_t>(offsets::Descriptors::NextBuff);

  uint32_t buff_id = GetRelative<uint32_t>(buff_offset);

  while (buff_id != 0) {
    buffs.push_back(buff_id);
    buff_offset += next_offset;
    buff_id = GetRelative<uint32_t>(buff_offset);
  }

  return buffs;
}

std::vector<uint32_t> WowUnit::GetDebuffIds() const
{
  std::vector<uint32_t> debuffs;

  ptrdiff_t debuff_offset =
    offsets::ObjectManagerOffsets::DescriptorOffset +
    static_cast<ptrdiff_t>(offsets::Descriptors::FirstDebuff);

  ptrdiff_t const next_offset =
    static_cast<ptrdiff_t>(offsets::Descriptors::NextBuff);

  uint32_t debuff_id = GetRelative<uint32_t>(debuff_offset);

  while (debuff_id != 0) {
    debuffs.push_back(debuff_id);
    debuff_offset += next_offset;
    debuff_id = GetRelative<uint32_t>(debuff_offset);
  }

  return debuffs;
}

void WowUnit::PrintToStream(std::ostream& os) const
{
  Vec3 const& pos = GetMovement()->position;

  os << std::hex << std::setfill('0');
  os << "{ type: WowUnit";
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
  os << ", movement_flags: " << std::setw(8) << GetMovement()->move_flags;
  os << ", dynamic_flags: " << std::setw(8) << GetDynamicFlags();
  os << " }";
}
}
