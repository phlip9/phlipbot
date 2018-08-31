#include "WowUnit.hpp"

#include <iomanip>
#include <iostream>
#include <stdint.h>
#include <string>
#include <vector>

using std::ostream;
using std::string;
using std::vector;

using phlipbot::memory::ReadCStr;
using phlipbot::memory::ReadRaw;
using phlipbot::offsets::Descriptors;
namespace ObjectManagerOffsets = phlipbot::offsets::ObjectManagerOffsets;

namespace phlipbot
{
string WowUnit::GetName() const
{
  uintptr_t const name_ptr = GetDescriptor<uintptr_t>(Descriptors::UnitNamePtr);
  if (!name_ptr) return "";

  uintptr_t const str_ptr = ReadRaw<uintptr_t>(name_ptr);
  if (!str_ptr) return "";

  // TODO(phlip9): need to reverse actual max size
  return ReadCStr(str_ptr, 0x40);
}

ObjectType WowUnit::GetObjectType() const { return ObjectType::UNIT; }

vector<uint32_t> WowUnit::GetBuffIds() const
{
  vector<uint32_t> buffs;

  ptrdiff_t buff_offset = ObjectManagerOffsets::DescriptorOffset +
                          static_cast<ptrdiff_t>(Descriptors::FirstBuff);

  ptrdiff_t const next_offset = static_cast<ptrdiff_t>(Descriptors::NextBuff);

  uint32_t buff_id = GetRelative<uint32_t>(buff_offset);

  while (buff_id != 0) {
    buffs.push_back(buff_id);
    buff_offset += next_offset;
    buff_id = GetRelative<uint32_t>(buff_offset);
  }

  return buffs;
}

vector<uint32_t> WowUnit::GetDebuffIds() const
{
  vector<uint32_t> debuffs;

  ptrdiff_t debuff_offset = ObjectManagerOffsets::DescriptorOffset +
                            static_cast<ptrdiff_t>(Descriptors::FirstDebuff);

  ptrdiff_t const next_offset = static_cast<ptrdiff_t>(Descriptors::NextBuff);

  uint32_t debuff_id = GetRelative<uint32_t>(debuff_offset);

  while (debuff_id != 0) {
    debuffs.push_back(debuff_id);
    debuff_offset += next_offset;
    debuff_id = GetRelative<uint32_t>(debuff_offset);
  }

  return debuffs;
}

void WowUnit::PrintToStream(ostream& os) const
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
