#pragma once

#include <optional>
#include <memory>

#include "../PlayerController.hpp"
#include "MoveMap.hpp"
#include "PathFinder.hpp"

namespace phlipbot
{
struct PlayerNavigator {
  explicit PlayerNavigator(ObjectManager& objmgr,
                           PlayerController& player_controller,
                           MMapManager& mmap_mgr) noexcept
    : objmgr(objmgr),
      player_controller(player_controller),
      mmap_mgr(mmap_mgr){};
  ~PlayerNavigator() = default;
  PlayerNavigator(PlayerNavigator const&) = delete;
  PlayerNavigator& operator=(PlayerNavigator const&) = delete;

  void SetDestination(vec3 const dest);
  void SetEnabled(bool val);
  void Update();

  ObjectManager& objmgr;
  PlayerController& player_controller;
  MMapManager& mmap_mgr;

  bool enabled{false};
  bool update_path{false};

  std::optional<PathFinder> path_info;
  size_t path_idx{0};
  vec3 destination{0, 0, 0};
};
}
