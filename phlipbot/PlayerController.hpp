#pragma once

#include <variant>

#include "ObjectManager.hpp"
#include "PID.hpp"
#include "wow_constants.hpp"

namespace phlipbot
{
struct PlayerController {
  using FacingSetpointT = std::variant<float, vec3, Guid>;

  explicit PlayerController(ObjectManager& om) noexcept;
  ~PlayerController() = default;

  PlayerController(PlayerController const&) = delete;
  PlayerController& operator=(PlayerController const&) = delete;

  void Update(float const dt);
  void Reset();

  inline void SetFacing(float const target_direction)
  {
    facing_setpoint = target_direction;
  }
  inline void SetFacing(vec3 const& target_point)
  {
    facing_setpoint = target_point;
  }
  inline void SetFacing(Guid const target_guid)
  {
    facing_setpoint = target_guid;
  }
  inline void SetFacing(FacingSetpointT const& setpoint)
  {
    facing_setpoint = setpoint;
  }
  inline void SetEnabled(bool const _enabled)
  {
    enabled = _enabled;
    Reset();
  }

  float ComputeFacing(FacingSetpointT const& setpoint) const;

  ObjectManager& objmgr;

  FacingSetpointT facing_setpoint{0.0f};

  PID facing_controller{};
  bool enabled{false};
};
}
