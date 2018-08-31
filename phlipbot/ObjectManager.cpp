#include "ObjectManager.hpp"

#include <inttypes.h>
#include <map>
#include <memory>

#include <boost/optional.hpp>

#include <hadesmem/detail/trace.hpp>

#include "WowContainer.hpp"
#include "WowGameObject.hpp"
#include "WowItem.hpp"
#include "WowObject.hpp"
#include "WowPlayer.hpp"
#include "WowUnit.hpp"
#include "wow_constants.hpp"

// TODO(phlip9): possibe to use boost::bind to bind object manager instance
//               to EnumVisibleObjects_Callback?
// TODO(phlip9): Reverse ClntObjMgr struct and iterate over it directly so
//               we can remove the global singleton.

using namespace phlipbot::types;

using ClntObjMgr__GetActivePlayer_Fn = Guid(__stdcall*)();

using ClntObjMgr__ObjectPtr_Fn = uintptr_t(__fastcall*)(uint32_t filter,
                                                        const char* fileName,
                                                        Guid guid,
                                                        uint32_t lineNum);

using ClntObjMgr__EnumVisibleObjects_Callback_Fn =
  uint32_t(__thiscall*)(uint32_t filter, Guid guid);

using ClntObjMgr__EnumVisibleObjects_Fn = uint32_t(__fastcall*)(
  ClntObjMgr__EnumVisibleObjects_Callback_Fn callback, uint32_t filter);

static inline Guid ClntObjMgr__GetActivePlayer()
{
  auto const getActivePlayerFn =
    reinterpret_cast<ClntObjMgr__GetActivePlayer_Fn>(
      phlipbot::offsets::Functions::ClntObjMgr__GetActivePlayer);

  return (getActivePlayerFn)();
}

static inline uintptr_t ClntObjMgr__ObjectPtr(uint32_t filter, Guid guid)
{
  auto const objectPtrFn = reinterpret_cast<ClntObjMgr__ObjectPtr_Fn>(
    phlipbot::offsets::Functions::ClntObjMgr__ObjectPtr);

  return (objectPtrFn)(filter, nullptr, guid, 0);
}

struct ThisT {
  uint32_t __thiscall ClntObjMgr__EnumVisibleObjects_Callback(Guid guid)
  {
    auto const filter = reinterpret_cast<uint32_t>(this);

    if (!guid) {
      // stop enumerating
      return 0;
    }

    uintptr_t const obj_ptr = ClntObjMgr__ObjectPtr(filter, guid);
    if (!obj_ptr) {
      // stop enumerating
      return 0;
    }

    uintptr_t const obj_type_ptr =
      obj_ptr + phlipbot::offsets::ObjectManagerOffsets::ObjType;

    uint8_t const obj_type_raw =
      phlipbot::memory::ReadRaw<uint8_t>(obj_type_ptr);

    ObjectType const obj_type = static_cast<ObjectType>(obj_type_raw);

    // HADESMEM_DETAIL_TRACE_FORMAT_A("guid: %#018" PRIx64 ", obj_ptr: %#10"
    //                               PRIx32 ", obj_type_raw: %#4" PRIx8,
    //                               guid, obj_ptr, obj_type_raw);

    HADESMEM_DETAIL_ASSERT(obj_type_raw <=
                           static_cast<uint8_t>(ObjectType::CORPSE));

    std::unique_ptr<phlipbot::WowObject> obj = nullptr;

    if (obj_type == ObjectType::NONE) {
      obj = std::make_unique<phlipbot::WowObject>(guid, obj_ptr);
    } else if (obj_type == ObjectType::ITEM) {
      obj = std::make_unique<phlipbot::WowItem>(guid, obj_ptr);
    } else if (obj_type == ObjectType::CONTAINER) {
      obj = std::make_unique<phlipbot::WowContainer>(guid, obj_ptr);
    } else if (obj_type == ObjectType::UNIT) {
      obj = std::make_unique<phlipbot::WowUnit>(guid, obj_ptr);
    } else if (obj_type == ObjectType::PLAYER) {
      obj = std::make_unique<phlipbot::WowPlayer>(guid, obj_ptr);
    } else if (obj_type == ObjectType::GAMEOBJ) {
      obj = std::make_unique<phlipbot::WowGameObject>(guid, obj_ptr);
    } else if (obj_type == ObjectType::DYNOBJ) {
      // skip
    } else if (obj_type == ObjectType::CORPSE) {
      // skip
    } else {
      // unreachable
    }

    phlipbot::ObjectManager& omgr = phlipbot::ObjectManager::Get();
    auto& guid_obj_cache = omgr.guid_obj_cache;

    if (obj) {
      // cache takes ownership of handle
      guid_obj_cache.emplace(guid, std::move(obj));
    }

    // continue
    return 1;
  }
};

static inline uint32_t ClntObjMgr__EnumVisibleObjects(uint32_t filter)
{
  auto const enumVisibleObjectsFn =
    reinterpret_cast<ClntObjMgr__EnumVisibleObjects_Fn>(
      phlipbot::offsets::Functions::ClntObjMgr__EnumVisibleObjects);

  auto original_callback_ptr = &ThisT::ClntObjMgr__EnumVisibleObjects_Callback;
  auto const callback_ptr =
    *reinterpret_cast<ClntObjMgr__EnumVisibleObjects_Callback_Fn*>(
      &original_callback_ptr);

  return (enumVisibleObjectsFn)(callback_ptr, filter);
}


namespace phlipbot
{
Guid ObjectManager::GetPlayerGuid() const
{
  return ClntObjMgr__GetActivePlayer();
}

boost::optional<WowPlayer*> ObjectManager::GetPlayer()
{
  Guid player_guid = GetPlayerGuid();
  if (player_guid == 0) return boost::none;

  auto o_obj = GetObjByGuid(player_guid);
  if (!o_obj) return boost::none;

  auto obj = *o_obj;
  HADESMEM_DETAIL_ASSERT(obj->GetObjectType() == ObjectType::PLAYER);

  return dynamic_cast<WowPlayer*>(obj);
}

void ObjectManager::EnumVisibleObjects()
{
  // HADESMEM_DETAIL_TRACE_A("Updating ObjectManager objects cache");

  // reset the guid to obj cache
  guid_obj_cache.clear();

  // Add all visible objects to the cache
  ClntObjMgr__EnumVisibleObjects(ObjectFilter::ALL);
}

boost::optional<WowObject*> ObjectManager::GetObjByGuid(types::Guid const guid)
{
  // check guid obj cache
  auto obj_iter = guid_obj_cache.find(guid);
  if (obj_iter != end(guid_obj_cache) && obj_iter->second) {
    return obj_iter->second.get();
  }

  // no obj with this guid
  return boost::none;
}

template <>
bool ObjectManager::is_obj_type<WowObject>(WowObject const* obj)
{
  return obj->GetObjectType() == ObjectType::NONE;
}

template <>
bool ObjectManager::is_obj_type<WowGameObject>(WowObject const* obj)
{
  return obj->GetObjectType() == ObjectType::GAMEOBJ;
}

template <>
bool ObjectManager::is_obj_type<WowItem>(WowObject const* obj)
{
  return obj->GetObjectType() == ObjectType::ITEM;
}

template <>
bool ObjectManager::is_obj_type<WowContainer>(WowObject const* obj)
{
  return obj->GetObjectType() == ObjectType::CONTAINER;
}

template <>
bool ObjectManager::is_obj_type<WowUnit>(WowObject const* obj)
{
  return obj->GetObjectType() == ObjectType::UNIT;
}

template <>
bool ObjectManager::is_obj_type<WowPlayer>(WowObject const* obj)
{
  return obj->GetObjectType() == ObjectType::PLAYER;
}

// TODO(phlip9): implement iterators specialized to units, gameobjects, etc...
}