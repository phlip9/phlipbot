#pragma once

#include <unordered_map>
#include <mutex>

#include "wow_constants.hpp"
#include "WowObject.hpp"

namespace phlipbot
{
struct ObjectManager
{
public:
  ObjectManager() = default;

  static inline ObjectManager& Get()
  {
    std::call_once(ObjectManager::onceFlag, []() {
      instance.reset(new ObjectManager);
    });
    return *(instance.get());
  }

  std::unordered_map<types::Guid, WowObject> guid_obj_cache;
  std::vector<WowObject> objs;

  phlipbot::types::Guid GetPlayerGuid();
  //WowObject const& GetObjByGuid(phlipbot::types::Guid const guid);
private:
  ObjectManager(const ObjectManager&) = delete;
  ObjectManager& operator=(const ObjectManager&) = delete;

  static std::unique_ptr<ObjectManager> instance;
  static std::once_flag onceFlag;
};
}