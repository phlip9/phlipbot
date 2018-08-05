#include "ObjectManager.hpp"

#include <map>
#include <memory>

// TODO(phlip9): remove
#include <iostream>

#include <boost/optional.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/map.hpp>

#include <hadesmem/detail/trace.hpp>

#include "wow_constants.hpp"
#include "WowObject.hpp"
#include "WowGameObject.hpp"
#include "WowItem.hpp"
#include "WowContainer.hpp"
#include "WowUnit.hpp"
#include "WowPlayer.hpp"

// TODO(phlip9): implement iterating through ObjectManager WowObjects
// TODO(phlip9): fill out other WowObject types (Item, Container, Unit, Player)

using namespace phlipbot::types;
using namespace boost::adaptors;

using ClntObjMgr__GetActivePlayer_Fn = Guid(__stdcall *)();

using ClntObjMgr__ObjectPtr_Fn =
  uintptr_t(__fastcall *)(
    uint32_t filter,
    const char* fileName,
    Guid guid,
    uint32_t lineNum);

using ClntObjMgr__EnumVisibleObjects_Callback_Fn =
  uint32_t(__thiscall *)(uint32_t filter, Guid guid);

using ClntObjMgr__EnumVisibleObjects_Fn =
  uint32_t(__fastcall *)(
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
  auto const objectPtrFn =
    reinterpret_cast<ClntObjMgr__ObjectPtr_Fn>(
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
    
    ObjectType const obj_type =
      static_cast<ObjectType>(phlipbot::memory::ReadRaw<uint8_t>(obj_type_ptr));

    phlipbot::ObjectManager& omgr = phlipbot::ObjectManager::Get();
    auto& guid_obj_cache = omgr.guid_obj_cache;

    if (obj_type == ObjectType::NONE) {
      guid_obj_cache.insert({ guid, phlipbot::WowObject{ guid, obj_ptr } });
    } else if (obj_type == ObjectType::ITEM) {
      guid_obj_cache.insert({ guid, phlipbot::WowUnit{ guid, obj_ptr } });
    } else if (obj_type == ObjectType::CONTAINER) {
      guid_obj_cache.insert({ guid, phlipbot::WowContainer{ guid, obj_ptr } });
    } else if (obj_type == ObjectType::UNIT) {
      guid_obj_cache.insert({ guid, phlipbot::WowUnit{ guid, obj_ptr } });
    } else if (obj_type == ObjectType::PLAYER) {
      guid_obj_cache.insert({ guid, phlipbot::WowPlayer{ guid, obj_ptr } });
    } else if (obj_type == ObjectType::GAMEOBJ) {
      guid_obj_cache.insert({ guid, phlipbot::WowGameObject{ guid, obj_ptr } });
    } else if (obj_type == ObjectType::DYNOBJ) {
      // skip
    } else if (obj_type == ObjectType::CORPSE) {
      // skip
    } else {
      // TODO(phlip9): Error, unknown object type
      // TODO(phlip9): Can we throw in this context?
      HADESMEM_DETAIL_THROW_EXCEPTION(
        hadesmem::Error{} << hadesmem::ErrorString{ "Unknown ObjectType" });
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
Guid ObjectManager::GetPlayerGuid()
{
  return ClntObjMgr__GetActivePlayer();
}

boost::optional<WowPlayer const&>
ObjectManager::GetPlayer()
{
  Guid player_guid = GetPlayerGuid();
  if (!player_guid) return boost::none;

  auto o_obj = GetObjByGuid(player_guid);
  if (!o_obj) return boost::none;

  auto const& obj = *o_obj;
  
  HADESMEM_DETAIL_ASSERT(obj.obj_type == ObjectType::PLAYER);

  return dynamic_cast<WowPlayer const&>(obj);
}

void ObjectManager::EnumVisibleObjects()
{
  // reset the guid to obj cache
  guid_obj_cache.clear();

  // Add all visible objects to the cache
  ClntObjMgr__EnumVisibleObjects(ObjectFilter::ALL);
}

boost::optional<WowObject const&>
ObjectManager::GetObjByGuid(types::Guid const guid)
{
  // check guid obj cache
  auto obj_iter = guid_obj_cache.find(guid);
  if (obj_iter != end(guid_obj_cache)) {
    return obj_iter->second;
  }

  // no obj with this guid
  return boost::none;
}

template <>
bool ObjectManager::is_obj_type<WowObject>(WowObject const& obj)
{ return obj.obj_type == ObjectType::NONE; }

template <>
bool ObjectManager::is_obj_type<WowGameObject>(WowObject const& obj)
{ return obj.obj_type == ObjectType::GAMEOBJ; }

template <>
bool ObjectManager::is_obj_type<WowItem>(WowObject const& obj)
{ return obj.obj_type == ObjectType::ITEM; }

template <>
bool ObjectManager::is_obj_type<WowContainer>(WowObject const& obj)
{ return obj.obj_type == ObjectType::CONTAINER; }

template <>
bool ObjectManager::is_obj_type<WowUnit>(WowObject const& obj)
{ return obj.obj_type == ObjectType::UNIT; }

template <>
bool ObjectManager::is_obj_type<WowPlayer>(WowObject const& obj)
{ return obj.obj_type == ObjectType::PLAYER; }

template <typename WowT>
ObjectManager::Range<WowT> ObjectManager::IterObjs()
{
  return guid_obj_cache | map_values | filtered(is_obj_type<WowT>);
}

void ObjectManager::Test()
{
  for (auto& obj : ObjectManager::IterObjs<WowUnit>()) {
    std::cout << obj.GetName() << std::endl;
  }
}

// TODO(phlip9): implement iterators specialized to units, gameobjects, etc...

}