#pragma once

#include "ObjectManager.hpp"
#include "PID.hpp"
#include "wow_constants.hpp"

namespace phlipbot
{
struct PlayerController {
  explicit PlayerController(ObjectManager& om) noexcept;
  ~PlayerController() = default;

  PlayerController(PlayerController const&) = delete;
  PlayerController& operator=(PlayerController const&) = delete;

  void Update(float const dt);
  void Reset();

  void SetFacingSetpoint(float const facing);
  void SetFacingTargetSetpoint(Vec3 const& target_point);
  void SetEnabled(bool const enabled);

  enum class FacingType { Facing, TargetPoint };

  ObjectManager& objmgr;

  float facing_setpoint{0.0f};
  Vec3 facing_target_setpoint{0, 0, 0};
  PID facing_controller{};

  bool enabled{false};
  FacingType facing_type{FacingType::Facing};
};
}
