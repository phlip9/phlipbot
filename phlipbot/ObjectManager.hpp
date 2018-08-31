#pragma once

#include <mutex>
#include <unordered_map>


#include <boost/optional.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/any_range.hpp>

#include "wow_constants.hpp"

#include "WowContainer.hpp"
#include "WowGameObject.hpp"
#include "WowItem.hpp"
#include "WowObject.hpp"
#include "WowPlayer.hpp"
#include "WowUnit.hpp"

namespace phlipbot
{
struct ObjectManager {
public:
  static inline ObjectManager& Get() noexcept
  {
    static ObjectManager instance;
    return instance;
  }

  inline bool IsIngame() { return static_cast<bool>(GetPlayer()); }

  template <typename WowT>
  static bool is_obj_type(phlipbot::WowObject const* obj);

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
  phlipbot::types::Guid GetPlayerGuid() const;

  boost::optional<phlipbot::WowPlayer*> GetPlayer();
  boost::optional<phlipbot::WowObject*>
  GetObjByGuid(phlipbot::types::Guid const guid);

  std::unordered_map<types::Guid, std::unique_ptr<WowObject>> guid_obj_cache;

private:
  ObjectManager() = default;
  ~ObjectManager() = default;
  ObjectManager(const ObjectManager&) = delete;
  ObjectManager& operator=(const ObjectManager&) = delete;
};
}