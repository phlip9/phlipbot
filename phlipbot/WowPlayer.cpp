#include "WowPlayer.hpp"

#include <Windows.h>
#include <cmath>
#include <iomanip>
#include <stdint.h>

#include <boost/math/constants/constants.hpp>

#include <hadesmem/detail/trace.hpp>

#include "memory.hpp"

using std::atan2f;
using std::ostream;
using std::string;

using boost::math::float_constants::pi;

using phlipbot::CMovementData;
using phlipbot::Guid;
using phlipbot::vec3;
using phlipbot::memory::ReadCStr;
using phlipbot::memory::ReadRaw;

namespace FunctionOffsets = phlipbot::offsets::FunctionOffsets;
namespace DataOffsets = phlipbot::offsets::Data;

using CGPlayer_C__ClickToMove_Fn = char(__thiscall*)(uintptr_t player,
                                                     uint32_t const ctm_type,
                                                     Guid const* target_guid,
                                                     vec3 const* target_pos,
                                                     float const precision);

using CMovement__SetFacing_Fn = uint32_t(__thiscall*)(CMovementData* move_ptr,
                                                      float const facing);

using CUnit_C__SendMovementUpdate_Fn = void(__thiscall*)(uintptr_t player,
                                                         uint32_t timestamp,
                                                         uint32_t opcode,
                                                         float _unk,
                                                         uint32_t __unk);

using CGInputControl__GetActive_Fn = uintptr_t(__stdcall*)();
using CGInputControl__SetControlBit_Fn =
  uint32_t(__thiscall*)(uintptr_t input_ctrl_ptr,
                        uint32_t control_flags,
                        uint32_t state,
                        uint32_t timestamp,
                        uint32_t sticky);

namespace
{
inline uintptr_t CGInputControl__GetActive()
{
  auto const get_active_fn = reinterpret_cast<CGInputControl__GetActive_Fn>(
    FunctionOffsets::CGInputControl__GetActive);

  return (get_active_fn)();
}
}

namespace phlipbot
{
string WowPlayer::GetName() const
{
  uintptr_t const cache_ptr = DataOffsets::DBCache__NameCache;

  uintptr_t entry_ptr = cache_ptr + 0x8;
  Guid next_guid = ReadRaw<Guid>(entry_ptr + 0xC);

  while (next_guid != 0) {
    if (next_guid == guid) {
      return ReadCStr(entry_ptr + 0x14, 0x40);
    }

    entry_ptr = ReadRaw<uintptr_t>(entry_ptr);
    next_guid = ReadRaw<Guid>(entry_ptr + 0xC);
  }

  return "";
}

ObjectType WowPlayer::GetObjectType() const { return ObjectType::PLAYER; }


void WowPlayer::PrintToStream(ostream& os) const
{
  vec3 const& pos = GetMovement()->position;

  os << std::hex << std::setfill('0');
  os << "{ type: WowPlayer";
  os << ", guid: 0x" << std::setw(16) << guid;
  os << ", base_ptr: 0x" << std::setw(8) << base_ptr;
  os << ", name: " << GetName();
  os << ", position: { " << pos.x << ", " << pos.y << ", " << pos.z << " } ";
  os << std::dec;
  os << ", npc_id: " << GetNpcId();
  os << ", faction_id: " << GetFactionId();
  os << ", health: " << GetHealthPercent() << "%";
  os << ", mana: " << GetManaPercent() << "%";
  os << ", rage: " << GetRage();
  os << std::hex;
  os << ", target: " << std::setw(16) << GetTargetGuid();
  os << ", movement_flags: " << std::setw(8) << GetMovement()->move_flags;
  os << ", dynamic_flags: " << std::setw(8) << GetDynamicFlags();
  os << " }";
}

void WowPlayer::SendUpdateMovement(uint32_t timestamp, MovementOpCode opcode)
{
  auto const send_movement_fn =
    reinterpret_cast<CUnit_C__SendMovementUpdate_Fn>(
      FunctionOffsets::CUnit_C__SendMovementUpdate);

  auto const _opcode = static_cast<uint32_t>(opcode);

  (send_movement_fn)(base_ptr, timestamp, _opcode, 0.0f, 0);
}

void WowPlayer::SetFacing(float facing_rad)
{
  auto const set_facing_fn = reinterpret_cast<CMovement__SetFacing_Fn>(
    FunctionOffsets::CMovement__SetFacing);

  (set_facing_fn)(GetMovement(), facing_rad);
}

void WowPlayer::SetFacing(vec3 const& target_pos)
{
  auto const& pos = GetMovement()->position;
  float const dy = target_pos.y - pos.y;
  float const dx = target_pos.x - pos.x;

  float const facing_rad = pi - atan2f(dy, -dx);

  SetFacing(facing_rad);

  SendUpdateMovement(::GetTickCount(), MovementOpCode::setFacing);
}

bool WowPlayer::ClickToMove(CtmType const ctm_type,
                            Guid const target_guid,
                            vec3 const& target_pos,
                            float const precision)
{
  auto const ctm_fn = reinterpret_cast<CGPlayer_C__ClickToMove_Fn>(
    FunctionOffsets::CGPlayer_C__ClickToMove);

  SetFacing(target_pos);

  return (ctm_fn)(base_ptr, static_cast<uint32_t>(ctm_type), &target_guid,
                  &target_pos, precision);
}

uint32_t WowPlayer::SetControlBits(uint32_t control_flags, uint32_t timestamp)
{
  auto const set_ctrl_bit_fn =
    reinterpret_cast<CGInputControl__SetControlBit_Fn>(
      FunctionOffsets::CGInputControl__SetControlBit);

  auto const input_ctrl_ptr = CGInputControl__GetActive();

  return (set_ctrl_bit_fn)(input_ctrl_ptr, control_flags, 1, timestamp, 0);
}

uint32_t WowPlayer::UnsetControlBits(uint32_t control_flags, uint32_t timestamp)
{
  auto const set_ctrl_bit_fn =
    reinterpret_cast<CGInputControl__SetControlBit_Fn>(
      FunctionOffsets::CGInputControl__SetControlBit);

  auto const input_ctrl_ptr = CGInputControl__GetActive();

  return (set_ctrl_bit_fn)(input_ctrl_ptr, control_flags, 0, timestamp, 0);
}
}