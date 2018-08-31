#include "PlayerController.hpp"

#include <cmath>

#include <boost/math/constants/constants.hpp>

#include <hadesmem/detail/assert.hpp>

#include "ObjectManager.hpp"

using std::atan2f;

using boost::math::float_constants::pi;
using boost::math::float_constants::two_pi;

namespace phlipbot
{
// TODO(phlip9): take in some config thing to set PID gains
PlayerController::PlayerController() noexcept {}

void PlayerController::Update(float const dt)
{
  if (!enabled) {
    return;
  }

  auto& objmgr = ObjectManager::Get();
  auto const o_player = objmgr.GetPlayer();
  if (!o_player.has_value()) {
    return;
  }
  auto* player = o_player.get();

  float const facing = player->GetMovement()->facing;
  float _facing_setpoint = 0.0f;

  if (facing_type == FacingType::Facing) {
    _facing_setpoint = facing_setpoint;
  } else if (facing_type == FacingType::TargetPoint) {
    // Get the angle between the player position and target position
    auto const& pos = player->GetMovement()->position;
    float const dy = facing_target_setpoint.Y - pos.Y;
    float const dx = facing_target_setpoint.X - pos.X;
    _facing_setpoint = pi - atan2f(dy, -dx);
  } else {
    HADESMEM_DETAIL_ASSERT(false && "invalid FacingType");
  }

  // Compute the shortest facing error, i.e., error turning cw vs error turning
  // ccw, whichever is closer to the setpoint.
  float const error1 = abs(_facing_setpoint - facing);
  float const error1_dir = (_facing_setpoint < facing) ? -1.0f : 1.0f;
  float const error2 = two_pi - error1;
  float const error2_dir = -error1_dir;
  float const facing_error = fminf(error1, error2);
  float const facing_dir = (error1 < error2) ? error1_dir : error2_dir;

  float const output = facing_controller.Update(facing_error, dt);

  // angular_acceleration = dir * output;
  // angular_velocity = prev_angular_velocity + angular_acceleration * dt;
  // prev_angular_velocity = angular_velocity;
  // float const new_facing =
  //   fmod(process_variable + angular_velocity * dt, two_pi);

  float const new_facing = fmod(facing + facing_dir * output, two_pi);
  player->SetFacing(new_facing);
}

void PlayerController::Reset() { facing_controller.Reset(); }

void PlayerController::SetFacingSetpoint(float const facing)
{
  HADESMEM_DETAIL_ASSERT(facing >= 0 && facing <= two_pi &&
                         "facing out of range");

  facing_setpoint = facing;
  facing_type = FacingType::Facing;
}

void PlayerController::SetFacingTargetSetpoint(Vec3 const& target_point)
{
  facing_target_setpoint = target_point;
  facing_type = FacingType::TargetPoint;
}

void PlayerController::SetEnabled(bool const _enabled)
{
  enabled = _enabled;
  Reset();
}
}