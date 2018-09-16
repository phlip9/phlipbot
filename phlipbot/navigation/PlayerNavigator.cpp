#include "PlayerNavigator.hpp"

#include <inttypes.h>

#include <glm/geometric.hpp>

#include <hadesmem/detail/trace.hpp>

using glm::length;

// TODO(phlip9): eventually update a pre-existing path if the destination
//               doesn't change too much
// TODO(phlip9): check if a tile is in-bounds before trying to load it
// TODO(phlip9): for now, assume destination is close, but we will eventually
//               have to load more intelligently


namespace phlipbot
{
void PlayerNavigator::SetDestination(vec3 const dest)
{
  update_path = true;
  path_info = std::nullopt;
  path_idx = 0;
  destination = dest;
}

void PlayerNavigator::SetEnabled(bool val)
{
  enabled = val;
  player_controller.SetEnabled(val);
}

void PlayerNavigator::Update()
{
  if (!enabled) {
    return;
  }

  auto oplayer = objmgr.GetPlayer();
  if (!oplayer.has_value()) {
    return;
  }

  auto const* player = oplayer.get();
  auto const player_pos = player->GetPosition();

  if (update_path) {
    auto const map_id = objmgr.GetMapId();
    vec2i const tile = mmap_mgr.tileFromPos(player_pos.xy);

    // load the tiles around the player
    bool any_success = false;
    for (int i = -1; i <= 1; ++i) {
      for (int j = -1; j <= 1; ++j) {
        vec2i const offset{i, j};
        any_success |= mmap_mgr.loadMap(map_id, tile + offset);
      }
    }

    if (!any_success) {
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Failed to load any tiles around tile {%d, %d}", tile.x, tile.y);
      return;
    }

    // calculate the path
    path_info.emplace(mmap_mgr, map_id);
    bool res = path_info->calculate(player_pos, destination);
    auto type = path_info->getPathType();
    if (!res || !type.test(PathFlag::PATHFIND_NORMAL)) {
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "path_info->calculate failed, path_info->GetPathType() = %s",
        path_info->getPathType().to_string().c_str());
      return;
    }

    update_path = false;
  }

  // move towards the next waypoint
  if (path_info.has_value() &&
      path_info->getPathType().test(PathFlag::PATHFIND_NORMAL)) {
    auto const& path = path_info->getPath();
    uint32_t const prev_path_idx = path_idx;

    // walk the path_idx forward until we hit the next position we need to reach
    while (path_idx < path.size() &&
           player_controller.InRange(player_pos, path[path_idx])) {
      path_idx += 1;
    }

    // set the player controller's position objective
    if (path_idx > prev_path_idx && path_idx < path.size()) {
      auto const& next_pos = path[path_idx];
      player_controller.SetPosition(next_pos);
      player_controller.SetFacing(next_pos);
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Moving to next waypoint {%.03f, %.03f %0.3f}", next_pos.x, next_pos.y,
        next_pos.z);
    }
  }
}
}