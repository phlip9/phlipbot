#include "ObjectManager.hpp"

#include "wow_constants.hpp"
#include "WowObject.hpp"

// TODO(phlip9): implement iterating through ObjectManager WowObjects
// TODO(phlip9): fill out other WowObject types (Item, Container, Unit, Player)

using namespace phlipbot::types;
using namespace phlipbot::offsets;

using ClntObjMgrGetActivePlayer_Fn = Guid(__stdcall *)();

using ClntObjMgrObjectPtr_Fn =
  uintptr_t(__fastcall *)(
    uint32_t filter,
    const char* fileName,
    Guid guid,
    uint32_t lineNum);

using ClntObjMgrEnumVisibleObjects_Callback_Fn =
  uint32_t(__thiscall *)(uint32_t filter, Guid guid);

using ClntObjMgrEnumVisibleObjects_Fn =
  uint32_t(__fastcall *)(
    ClntObjMgrEnumVisibleObjects_Callback_Fn callback, uint32_t filter);

static inline Guid ClntObjMgrGetActivePlayer() {
  uintptr_t const offset = static_cast<uintptr_t>(
    phlipbot::offsets::Functions::ClntObjMgrGetActivePlayer);

  auto const getActivePlayerFn =
    reinterpret_cast<ClntObjMgrGetActivePlayer_Fn>(offset);

  return (getActivePlayerFn)();
}

static inline uintptr_t ClntObjMgrObjectPtr(uint32_t filter, Guid guid)
{
  uintptr_t const offset = static_cast<uintptr_t>(
    phlipbot::offsets::Functions::ClntObjMgrObjectPtr);

  auto const objectPtrFn =
    reinterpret_cast<ClntObjMgrObjectPtr_Fn>(offset);

  //uint32_t const filter = static_cast<uint32_t>(ObjectFilter::ALL);

  return (objectPtrFn)(filter, nullptr, guid, 0);
}

struct ThisT {
  uint32_t __thiscall ClntObjMgrEnumVisibleObjects_Callback(Guid guid)
  {
    auto const filter = reinterpret_cast<uint32_t>(this);

    return filter + (guid >> 32);
  }
};

static inline uint32_t ClntObjMgrEnumVisibleObjects(uint32_t filter)
{
  uintptr_t const offset = static_cast<uintptr_t>(
    phlipbot::offsets::Functions::ClntObjMgrEnumVisibleObjects);

  auto const enumVisibleObjectsFn =
    reinterpret_cast<ClntObjMgrEnumVisibleObjects_Fn>(offset);

  auto original_callback_ptr = &ThisT::ClntObjMgrEnumVisibleObjects_Callback;
  auto const callback_ptr =
    *reinterpret_cast<ClntObjMgrEnumVisibleObjects_Callback_Fn*>(
      &original_callback_ptr);

  return (enumVisibleObjectsFn)(callback_ptr, filter);
}

namespace phlipbot
{
Guid ObjectManager::GetPlayerGuid()
{
  return ClntObjMgrGetActivePlayer();
}

//WowObject const& ObjectManager::GetObjByGuid(types::Guid const guid)
//{
//
//}
}