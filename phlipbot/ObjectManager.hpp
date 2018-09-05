#pragma once

#include <unordered_map>

#include <boost/optional.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/any_range.hpp>

#include "WowContainer.hpp"
#include "WowGameObject.hpp"
#include "WowItem.hpp"
#include "WowObject.hpp"
#include "WowPlayer.hpp"
#include "WowUnit.hpp"
#include "wow_constants.hpp"

namespace phlipbot
{
struct ObjectManager {
public:
  explicit ObjectManager() = default;
  ~ObjectManager() = default;
  ObjectManager(const ObjectManager&) = delete;
  ObjectManager& operator=(const ObjectManager&) = delete;

  inline bool IsInGame() { return GetPlayerGuid() != 0; }

  template <typename WowT>
  static bool is_obj_type(WowObject const* obj);

  template <typename WowT>
  using Range = boost::any_range<WowT*, // value type
                                 boost::forward_traversal_tag, // iterator type
                                 WowT*, // reference type
                                 std::ptrdiff_t>;

  template <typename WowT>
  inline Range<WowT> IterObjs()
  {
    using namespace boost::adaptors;

    // clang-format off

    return guid_obj_cache
      | map_values
      | transformed([](std::unique_ptr<WowObject>& obj) { return obj.get(); })
      | filtered(is_obj_type<WowT>)
      | transformed([](WowObject* obj) { return dynamic_cast<WowT*>(obj); });

    // clang-format on
  }

  inline Range<WowObject> IterAllObjs()
  {
    using namespace boost::adaptors;

    // clang-format off

    return guid_obj_cache
      | map_values
      | transformed([](std::unique_ptr<WowObject>& obj) { return obj.get(); });

    // clang-format on
  }

  void EnumVisibleObjects();
  Guid GetPlayerGuid() const;

  boost::optional<WowPlayer*> GetPlayer();
  boost::optional<WowObject*> GetObjByGuid(Guid const guid);

  std::unordered_map<Guid, std::unique_ptr<WowObject>> guid_obj_cache{};
};
}