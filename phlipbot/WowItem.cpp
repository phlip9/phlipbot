#include "WowItem.hpp"

#include <string>

#include "memory.hpp"
#include "wow_constants.hpp"

using namespace phlipbot::types;

using DBCache__ItemStats_C__GetRecord_Fn =
  uintptr_t(__thiscall*)(uintptr_t dbcache_ptr,
                         uint32_t item_id,
                         Guid* guid_ptr,
                         uintptr_t callback,
                         uintptr_t callback_args,
                         uint32_t do_callback);

uintptr_t GetItemStatsPtrFromDBCache(uint32_t item_id)
{
  auto const get_record_fn =
    reinterpret_cast<DBCache__ItemStats_C__GetRecord_Fn>(
      phlipbot::offsets::Functions::DBCache__ItemStats_C__GetRecord);

  uintptr_t const item_cache_ptr =
    phlipbot::offsets::Data::DBCache__ItemStats_C;

  Guid guid{0};
  return (get_record_fn)(item_cache_ptr, item_id, &guid, 0, 0, 0);
}

namespace phlipbot
{
std::string WowItem::GetName()
{
  uint32_t const item_id = GetItemId();
  uintptr_t const item_stats_ptr = GetItemStatsPtrFromDBCache(item_id);

  // TODO(phlip9): If the item is not in the item cache, query the server first.
  if (!item_stats_ptr) return "";

  uintptr_t const name_ptr = phlipbot::memory::ReadRaw<uintptr_t>(
    item_stats_ptr + phlipbot::offsets::ItemStats::Name);

  if (!name_ptr) return "";

  return phlipbot::memory::ReadCStr(name_ptr, 0x40);
}

ItemQuality WowItem::GetQuality()
{
  uint32_t const item_id = GetItemId();
  uintptr_t const item_stats_ptr = GetItemStatsPtrFromDBCache(item_id);

  // TODO(phlip9): If the item is not in the item cache, query the server first.
  if (!item_stats_ptr) return ItemQuality::Grey;

  uint32_t const quality = phlipbot::memory::ReadRaw<uint32_t>(
    item_stats_ptr + phlipbot::offsets::ItemStats::Quality);

  return static_cast<ItemQuality>(quality);
}

ObjectType WowItem::GetObjectType() const { return ObjectType::ITEM; }
}