#include "ObjectManager.hpp"

#include <map>
#include <memory>

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

  //uint32_t const filter = static_cast<uint32_t>(ObjectFilter::ALL);

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

    std::unique_ptr<phlipbot::WowObject> obj;

    if (obj_type == ObjectType::NONE) {
      obj = std::make_unique<phlipbot::WowObject>(
        phlipbot::WowObject{ guid, obj_ptr });
    } else if (obj_type == ObjectType::ITEM) {
      obj = std::make_unique<phlipbot::WowUnit>(
        phlipbot::WowUnit{ guid, obj_ptr });
    } else if (obj_type == ObjectType::CONTAINER) {
      obj = std::make_unique<phlipbot::WowContainer>(
        phlipbot::WowContainer{ guid, obj_ptr });
    } else if (obj_type == ObjectType::UNIT) {
      obj = std::make_unique<phlipbot::WowUnit>(
        phlipbot::WowUnit{ guid, obj_ptr });
    } else if (obj_type == ObjectType::PLAYER) {
      obj = std::make_unique<phlipbot::WowPlayer>(
        phlipbot::WowPlayer{ guid, obj_ptr });
    } else if (obj_type == ObjectType::GAMEOBJ) {
      obj = std::make_unique<phlipbot::WowGameObject>(
        phlipbot::WowGameObject{ guid, obj_ptr });
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

    if (obj) {
      guid_obj_cache.insert({ guid, *obj });
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

//WowObject const& ObjectManager::GetObjByGuid(types::Guid const guid)
//{
//
//}
}