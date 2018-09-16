#pragma once

#include <variant>

#include "ObjectManager.hpp"
#include "PID.hpp"
#include "wow_constants.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace phlipbot
{
struct PlayerController {
  using FacingSetpoint = std::variant<float, vec3, Guid>;
  using PositionSetpoint = vec3;

  explicit PlayerController(ObjectManager& om) noexcept;
  ~PlayerController() = default;

  PlayerController(PlayerController const&) = delete;
  PlayerController& operator=(PlayerController const&) = delete;

  void Update(float const dt);
  void Reset();

  inline void SetFacing(float const target_dir)
  {
    facing_setpoint = target_dir;
  }
  inline void SetFacing(vec3 const& target_pos)
  {
    facing_setpoint = target_pos;
  }
  inline void SetFacing(Guid const target_guid)
  {
    facing_setpoint = target_guid;
  }
  inline void SetFacing(FacingSetpoint const& setpoint)
  {
    facing_setpoint = setpoint;
  }
  inline void SetPosition(PositionSetpoint const& target_pos)
  {
    position_setpoint = target_pos;
  }
  inline void SetEnabled(bool const _enabled)
  {
    enabled = _enabled;
    Reset();
  }

  float ComputeFacing(FacingSetpoint const& setpoint) const;

  inline bool InRange(vec3 const& p1, vec3 const& p2)
  {
    return glm::length(p1.xy - p2.xy) <= range_thresh_2d &&
           glm::abs(p1.z - p2.z) <= range_thresh_height;
  }

  float const range_thresh_2d = 0.4f;
  float const range_thresh_height = 3.0f;

  ObjectManager& objmgr;
  bool enabled{false};

  FacingSetpoint facing_setpoint{0.0f};
  PID facing_controller{0.15f, 0.0f, 0.0f};

  PositionSetpoint position_setpoint{0, 0, 0};
};
}
