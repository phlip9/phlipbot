#include "PlayerController.hpp"

#include <cmath>

#include <boost/math/constants/constants.hpp>

#include <hadesmem/detail/assert.hpp>

using std::abs;
using std::atan2;
using std::fmod;
using std::min;

using boost::math::float_constants::pi;
using boost::math::float_constants::two_pi;

using phlipbot::vec2;
using phlipbot::vec3;

namespace
{
template <class... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overload(Ts...)->overload<Ts...>;

float wow_angle_2d(vec3 const& a, vec3 const& b)
{
  vec2 d = (b - a).xy;
  return pi - atan2(d.y, -d.x);
}
}

namespace phlipbot
{
// TODO(phlip9): take in some config thing to set PID gains
PlayerController::PlayerController(ObjectManager& om) noexcept : objmgr(om) {}

void PlayerController::Update(float const dt)
{
  if (!enabled) {
    return;
  }

  auto const o_player = objmgr.GetPlayer();
  if (!o_player.has_value()) {
    return;
  }
  auto* player = o_player.get();

  float const facing = player->GetMovement()->facing;
  float _facing_setpoint = ComputeFacing(facing_setpoint);

  // Compute the shortest facing error, i.e., error turning cw vs error turning
  // ccw, whichever is closer to the setpoint.
  float const error1 = abs(_facing_setpoint - facing);
  float const error1_dir = (_facing_setpoint < facing) ? -1.0f : 1.0f;
  float const error2 = two_pi - error1;
  float const error2_dir = -error1_dir;
  float const facing_error = min(error1, error2);
  float const facing_dir = (error1 < error2) ? error1_dir : error2_dir;

  if (fabs(facing_error) >= 1e-2) {
    float const output = facing_controller.Update(facing_error, dt);
    float const new_facing = fmod(facing + facing_dir * output, two_pi);
    player->SetFacing(new_facing);
  }
}

float PlayerController::ComputeFacing(FacingSetpointT const& setpoint) const
{
  // clang-format off
  return std::visit(
    overload {
      [](float const target_facing)
      {
        return target_facing;
      },
      [&](vec3 const& target_pos)
      {
        auto oplayer = objmgr.GetPlayer();
        vec3 const& player_pos = oplayer.get()->GetPosition();
        return wow_angle_2d(player_pos, target_pos);
      },
      [&](Guid const target_guid)
      {
        auto oplayer = objmgr.GetPlayer();
        auto otarget = objmgr.GetObjByGuid(target_guid);
        vec3 const& target_pos = otarget.get()->GetPosition();
        vec3 const& player_pos = oplayer.get()->GetPosition();
        return wow_angle_2d(player_pos, target_pos);
      }
    },
    setpoint);
  // clang-format on
}

void PlayerController::Reset() { facing_controller.Reset(); }
}