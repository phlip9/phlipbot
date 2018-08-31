#pragma once

#include "PID.hpp"
#include "wow_constants.hpp"

namespace phlipbot
{
struct PlayerController {
  explicit PlayerController() noexcept;

  void Update(float const dt);
  void Reset();

  void SetFacingSetpoint(float const facing);
  void SetFacingTargetSetpoint(types::Vec3 const& target_point);
  void SetEnabled(bool const enabled);

  enum class FacingType { Facing, TargetPoint };

  bool enabled = false;
  FacingType facing_type = FacingType::Facing;
  float facing_setpoint = 0.0f;
  types::Vec3 facing_target_setpoint;
  PID facing_controller;
};
}
