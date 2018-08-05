#pragma once

#include <unordered_map>
#include <mutex>

#include <boost/optional.hpp>
#include <boost/range/any_range.hpp>

#include "wow_constants.hpp"

#include "WowObject.hpp"
#include "WowGameObject.hpp"
#include "WowItem.hpp"
#include "WowContainer.hpp"
#include "WowUnit.hpp"
#include "WowPlayer.hpp"

namespace phlipbot
{
struct ObjectManager
{
public:
  static inline ObjectManager& Get()
  {
    static ObjectManager instance;
    return instance;
  }

  inline bool IsIngame()
  {
    return static_cast<bool>(GetPlayer());
  }

  template <typename WowT>
  static bool is_obj_type(phlipbot::WowObject const& obj);

  template <typename WowT>
  using Range = boost::any_range<
    WowT&, // value type
    boost::forward_traversal_tag, // iterator type
    WowT&, // reference type
    std::ptrdiff_t>;

  template <typename WowT>
  Range<WowT> IterObjs();

  void Test();

  void EnumVisibleObjects();
  phlipbot::types::Guid GetPlayerGuid();

  boost::optional<phlipbot::WowPlayer const&> GetPlayer();
  boost::optional<phlipbot::WowObject const&> GetObjByGuid(phlipbot::types::Guid const guid);

  std::unordered_map<types::Guid, WowObject> guid_obj_cache;
  std::vector<WowObject> objs;

private:
  ObjectManager() = default;
  ~ObjectManager() = default;
  ObjectManager(const ObjectManager&) = delete;
  ObjectManager& operator=(const ObjectManager&) = delete;
};
}