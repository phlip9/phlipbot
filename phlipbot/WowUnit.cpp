#include "WowUnit.hpp"

#include <string>
#include <vector>
#include <stdint.h>

#include "memory.hpp"

namespace phlipbot
{
std::string WowUnit::GetName()
{
  uintptr_t const name_ptr =
    GetDescriptor<uintptr_t>(offsets::Descriptors::UnitNamePtr);
  if (!name_ptr) return "";

  uintptr_t const str_ptr = memory::ReadRaw<uintptr_t>(name_ptr);
  if (!str_ptr) return "";

  // TODO(phlip9): need to reverse actual max size
  return memory::ReadCStr(str_ptr, 0x40);
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
