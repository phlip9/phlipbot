#include "WowUnit.hpp"

#include <string>
#include <vector>
#include <stdint.h>

namespace phlipbot
{
std::string WowUnit::GetName()
{
  // TODO(phlip9): implement WowUnit::GetName()
  return "";
}

std::vector<uint32_t> WowUnit::GetBuffIds()
{
  std::vector<uint32_t> buffs;

  ptrdiff_t buff_offset =
    offsets::ObjectManagerOffsets::DescriptorOffset
    + static_cast<ptrdiff_t>(offsets::Descriptors::FirstBuff);

  ptrdiff_t next_offset =
    static_cast<ptrdiff_t>(offsets::Descriptors::NextBuff);

  uint32_t buff_id = GetRelative<uint32_t>(buff_offset);

  while (buff_id != 0) {
    buffs.push_back(buff_id);
    buff_offset += next_offset;
    buff_id = GetRelative<uint32_t>(buff_offset);
  }

  return buffs;
}

std::vector<uint32_t> WowUnit::GetDebuffIds()
{
  std::vector<uint32_t> debuffs;

  ptrdiff_t debuff_offset =
    offsets::ObjectManagerOffsets::DescriptorOffset
    + static_cast<ptrdiff_t>(offsets::Descriptors::FirstDebuff);

  ptrdiff_t next_offset =
    static_cast<ptrdiff_t>(offsets::Descriptors::NextBuff);

  uint32_t debuff_id = GetRelative<uint32_t>(debuff_offset);

  while (debuff_id != 0) {
    debuffs.push_back(debuff_id);
    debuff_offset += next_offset;
    debuff_id = GetRelative<uint32_t>(debuff_offset);
  }

  return debuffs;
}
}
