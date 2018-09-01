#include "WowItem.hpp"

#include <iomanip>

#include "memory.hpp"

using std::ostream;
using std::string;

using phlipbot::Guid;
using phlipbot::memory::ReadCStr;
using phlipbot::memory::ReadRaw;
using phlipbot::offsets::Descriptor;

namespace DataOffsets = phlipbot::offsets::Data;
namespace FunctionOffsets = phlipbot::offsets::FunctionOffsets;
namespace ItemStatsOffsets = phlipbot::offsets::ItemStatsOffsets;

using DBCache__ItemStats_C__GetRecord_Fn =
  uintptr_t(__thiscall*)(uintptr_t dbcache_ptr,
                         uint32_t item_id,
                         Guid* guid_ptr,
                         uintptr_t callback,
                         uintptr_t callback_args,
                         uint32_t do_callback);

namespace
{
uintptr_t GetItemStatsPtrFromDBCache(uint32_t item_id)
{
  auto const get_record_fn =
    reinterpret_cast<DBCache__ItemStats_C__GetRecord_Fn>(
      FunctionOffsets::DBCache__ItemStats_C__GetRecord);

  uintptr_t const item_cache_ptr = DataOffsets::DBCache__ItemStats_C;

  Guid guid{0};
  return (get_record_fn)(item_cache_ptr, item_id, &guid, 0, 0, 0);
}
}

namespace phlipbot
{
uint32_t WowItem::GetItemId() const
{
  return GetDescriptor<uint32_t>(Descriptor::ItemId);
}

string WowItem::GetName() const
{
  uint32_t const item_id = GetItemId();
  uintptr_t const item_stats_ptr = GetItemStatsPtrFromDBCache(item_id);

  // TODO(phlip9): If the item is not in the item cache, query the server first.
  if (!item_stats_ptr) return "";

  uintptr_t const name_ptr =
    ReadRaw<uintptr_t>(item_stats_ptr + ItemStatsOffsets::Name);

  if (!name_ptr) return "";

  return ReadCStr(name_ptr, 0x40);
}

ItemQuality WowItem::GetQuality() const
{
  uint32_t const item_id = GetItemId();
  uintptr_t const item_stats_ptr = GetItemStatsPtrFromDBCache(item_id);

  // TODO(phlip9): If the item is not in the item cache, query the server first.
  if (!item_stats_ptr) return ItemQuality::Grey;

  uint32_t const quality =
    ReadRaw<uint32_t>(item_stats_ptr + ItemStatsOffsets::Quality);

  return static_cast<ItemQuality>(quality);
}

void WowItem::PrintToStream(ostream& os) const
{
  os << std::hex << std::setfill('0');
  os << "{ type: WowItem";
  os << ", guid: 0x" << std::setw(16) << guid;
  os << ", base_ptr: 0x" << std::setw(8) << base_ptr;
  os << ", name: " << GetName();
  os << std::dec;
  os << ", item_id: " << GetItemId();
  os << ", count: " << GetStackCount();
  os << ", durability: " << GetDurabilityPercent();
  os << " }";
}

ObjectType WowItem::GetObjectType() const { return ObjectType::ITEM; }
}