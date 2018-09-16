#include "ObjectManager.hpp"

#include <inttypes.h>
#include <memory>

#include <hadesmem/detail/trace.hpp>

// TODO(phlip9): possibe to use boost::bind to bind object manager instance
//               to EnumVisibleObjects_Callback?
// TODO(phlip9): Reverse ClntObjMgr struct and iterate over it directly so
//               we can remove the global singleton.

using std::make_unique;
using std::unique_ptr;

using boost::none;
using boost::optional;

using phlipbot::Guid;
using phlipbot::ObjectManager;
using phlipbot::ObjectType;
using phlipbot::WowContainer;
using phlipbot::WowGameObject;
using phlipbot::WowItem;
using phlipbot::WowObject;
using phlipbot::WowPlayer;
using phlipbot::WowUnit;
using phlipbot::memory::ReadRaw;

namespace FunctionOffsets = phlipbot::offsets::FunctionOffsets;
namespace ObjectManagerOffsets = phlipbot::offsets::ObjectManagerOffsets;

using ClntObjMgr__GetActivePlayer_Fn = Guid(__stdcall*)();

using ClntObjMgr__GetMapId_Fn = uint32_t(__stdcall*)();

using ClntObjMgr__ObjectPtr_Fn = uintptr_t(__fastcall*)(uint32_t filter,
                                                        const char* fileName,
                                                        Guid guid,
                                                        uint32_t lineNum);

using ClntObjMgr__EnumVisibleObjects_Callback_Fn =
  uint32_t(__thiscall*)(uint32_t filter, Guid guid);

using ClntObjMgr__EnumVisibleObjects_Fn = uint32_t(__fastcall*)(
  ClntObjMgr__EnumVisibleObjects_Callback_Fn callback, uint32_t filter);

namespace
{
// A hack-ish way of passing the ObjectManager instance to
// ClntObjMgr__EnumVisibleObjects_Callback.
// NOTE: DO NOT USE FOR ANY OTHER REASON
// NOTE: Obviously not threadsafe
ObjectManager*& GetEnumVisibleCtxt() noexcept
{
  static ObjectManager* objmgr = nullptr;
  return objmgr;
}

void SetEnumVisibleCtxt(ObjectManager* objmgr) noexcept
{
  GetEnumVisibleCtxt() = objmgr;
}

inline Guid ClntObjMgr__GetActivePlayer()
{
  auto const getActivePlayerFn =
    reinterpret_cast<ClntObjMgr__GetActivePlayer_Fn>(
      FunctionOffsets::ClntObjMgr__GetActivePlayer);

  return (getActivePlayerFn)();
}

inline uint32_t ClntObjMgr__GetMapId()
{
  auto const getMapIdFn = reinterpret_cast<ClntObjMgr__GetMapId_Fn>(
    FunctionOffsets::ClntObjMgr__GetMapId);

  return (getMapIdFn)();
}

inline uintptr_t ClntObjMgr__ObjectPtr(uint32_t filter, Guid guid)
{
  auto const objectPtrFn = reinterpret_cast<ClntObjMgr__ObjectPtr_Fn>(
    FunctionOffsets::ClntObjMgr__ObjectPtr);

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

    uintptr_t const obj_type_ptr = obj_ptr + ObjectManagerOffsets::ObjType;
    uint8_t const obj_type_raw = ReadRaw<uint8_t>(obj_type_ptr);
    ObjectType const obj_type = static_cast<ObjectType>(obj_type_raw);

    // HADESMEM_DETAIL_TRACE_FORMAT_A("guid: %#018" PRIx64 ", obj_ptr: %#10"
    //                               PRIx32 ", obj_type_raw: %#4" PRIx8,
    //                               guid, obj_ptr, obj_type_raw);

    HADESMEM_DETAIL_ASSERT(obj_type_raw <=
                           static_cast<uint8_t>(ObjectType::CORPSE));

    std::unique_ptr<WowObject> obj = nullptr;

    if (obj_type == ObjectType::NONE) {
      obj = make_unique<WowObject>(guid, obj_ptr);
    } else if (obj_type == ObjectType::ITEM) {
      obj = make_unique<WowItem>(guid, obj_ptr);
    } else if (obj_type == ObjectType::CONTAINER) {
      obj = make_unique<WowContainer>(guid, obj_ptr);
    } else if (obj_type == ObjectType::UNIT) {
      obj = make_unique<WowUnit>(guid, obj_ptr);
    } else if (obj_type == ObjectType::PLAYER) {
      obj = make_unique<WowPlayer>(guid, obj_ptr);
    } else if (obj_type == ObjectType::GAMEOBJ) {
      obj = make_unique<WowGameObject>(guid, obj_ptr);
    } else if (obj_type == ObjectType::DYNOBJ) {
      // skip
    } else if (obj_type == ObjectType::CORPSE) {
      // skip
    } else {
      // unreachable
    }

    if (obj) {
      // Retrieve the callback context
      auto* objmgr = GetEnumVisibleCtxt();
      HADESMEM_DETAIL_ASSERT(objmgr && "SetEnumVisibleCtxt must called before "
                                       "calling into "
                                       "ClntObjMgr__EnumVisibleObjects");
      // cache takes ownership of handle
      objmgr->guid_obj_cache.emplace(guid, std::move(obj));
    }

    // continue
    return 1;
  }
};

inline uint32_t ClntObjMgr__EnumVisibleObjects(uint32_t filter)
{
  auto const enumVisibleObjectsFn =
    reinterpret_cast<ClntObjMgr__EnumVisibleObjects_Fn>(
      FunctionOffsets::ClntObjMgr__EnumVisibleObjects);

  auto original_callback_ptr = &ThisT::ClntObjMgr__EnumVisibleObjects_Callback;
  auto const callback_ptr =
    *reinterpret_cast<ClntObjMgr__EnumVisibleObjects_Callback_Fn*>(
      &original_callback_ptr);

  return (enumVisibleObjectsFn)(callback_ptr, filter);
}
}


namespace phlipbot
{
Guid ObjectManager::GetPlayerGuid() const
{
  return ClntObjMgr__GetActivePlayer();
}

uint32_t ObjectManager::GetMapId() const { return ClntObjMgr__GetMapId(); }

optional<WowPlayer*> ObjectManager::GetPlayer()
{
  Guid player_guid = GetPlayerGuid();
  if (player_guid == 0) return none;

  auto o_obj = GetObjByGuid(player_guid);
  if (!o_obj) return none;

  auto obj = o_obj.value();
  HADESMEM_DETAIL_ASSERT(obj->GetObjectType() == ObjectType::PLAYER);

  return dynamic_cast<WowPlayer*>(obj);
}

void ObjectManager::EnumVisibleObjects()
{
  // reset the guid to obj cache
  guid_obj_cache.clear();

  // Add all visible objects to the cache
  SetEnumVisibleCtxt(this);
  ClntObjMgr__EnumVisibleObjects(ObjectFilter::ALL);
  SetEnumVisibleCtxt(nullptr);
}

optional<WowObject*> ObjectManager::GetObjByGuid(Guid const guid)
{
  // check guid obj cache
  auto obj_iter = guid_obj_cache.find(guid);
  if (obj_iter != end(guid_obj_cache) && obj_iter->second) {
    return obj_iter->second.get();
  }

  // no obj with this guid
  return none;
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