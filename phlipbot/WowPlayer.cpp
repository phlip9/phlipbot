#include "WowPlayer.hpp"

#include <Windows.h>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <stdint.h>
#include <string>

#include <boost/math/constants/constants.hpp>

#include <hadesmem/detail/trace.hpp>

#include "memory.hpp"
#include "wow_constants.hpp"

using namespace phlipbot::types;
using namespace phlipbot::offsets::Data;

using CGPlayer_C__ClickToMove_Fn = char(__thiscall*)(uintptr_t player,
                                                     uint32_t const ctm_type,
                                                     Guid const* target_guid,
                                                     XYZ const* target_pos,
                                                     float const precision);

using CMovement__SetFacing_Fn = uint32_t(__thiscall*)(uintptr_t cmovement_data,
                                                      float const facing);

using CUnit_C__SendMovementUpdate_Fn = void(__thiscall*)(uintptr_t player,
                                                         uint32_t timestamp,
                                                         uint32_t opcode,
                                                         float _unk,
                                                         uint32_t __unk);

namespace phlipbot
{
std::string WowPlayer::GetName() const
{
  uintptr_t const cache_ptr = offsets::Data::DBCache__NameCache;

  uintptr_t entry_ptr = cache_ptr + 0x8;
  Guid next_guid = memory::ReadRaw<Guid>(entry_ptr + 0xC);

  while (next_guid != 0) {
    if (next_guid == guid) {
      return memory::ReadCStr(entry_ptr + 0x14, 0x40);
    }

    entry_ptr = memory::ReadRaw<uintptr_t>(entry_ptr);
    next_guid = memory::ReadRaw<Guid>(entry_ptr + 0xC);
  }

  return "";
}

ObjectType WowPlayer::GetObjectType() const { return ObjectType::PLAYER; }


void WowPlayer::PrintToStream(std::ostream& os) const
{
  XYZ pos = GetPosition();

  os << std::hex << std::setfill('0');
  os << "{ type: WowPlayer";
  os << ", guid: 0x" << std::setw(16) << guid;
  os << ", base_ptr: 0x" << std::setw(8) << base_ptr;
  os << ", name: " << GetName();
  os << ", position: { " << pos.X << ", " << pos.Y << ", " << pos.Z << " } ";
  os << std::dec;
  os << ", npc_id: " << GetNpcId();
  os << ", faction_id: " << GetFactionId();
  os << ", health: " << GetHealthPercent() << "%";
  os << ", mana: " << GetManaPercent() << "%";
  os << ", rage: " << GetRage();
  os << std::hex;
  os << ", target: " << std::setw(16) << GetTargetGuid();
  os << ", movement_flags: " << std::setw(8) << GetMovementFlags();
  os << ", dynamic_flags: " << std::setw(8) << GetDynamicFlags();
  os << " }";
}

void WowPlayer::SendUpdateMovement(uint32_t timestamp, MovementOpCode opcode)
{
  auto const send_movement_fn =
    reinterpret_cast<CUnit_C__SendMovementUpdate_Fn>(
      offsets::Functions::CUnit_C__SendMovementUpdate);

  auto const _opcode = static_cast<uint32_t>(opcode);

  (send_movement_fn)(base_ptr, timestamp, _opcode, 0.0f, 0);
}

void WowPlayer::SetFacing(float facing_rad)
{
  uintptr_t cmovement_data =
    base_ptr + static_cast<ptrdiff_t>(offsets::Descriptors::Unit_MovementC);

  auto const set_facing_fn = reinterpret_cast<CMovement__SetFacing_Fn>(
    offsets::Functions::CMovement__SetFacing);

  (set_facing_fn)(cmovement_data, facing_rad);
}

void WowPlayer::SetFacing(XYZ const& target_pos)
{
  using namespace boost::math::float_constants;

  auto const pos = GetPosition();
  float const dy = target_pos.Y - pos.Y;
  float const dx = target_pos.X - pos.X;

  float const facing_rad = pi - std::atan2f(dy, -dx);

  SetFacing(facing_rad);

  SendUpdateMovement(::GetTickCount(), MovementOpCode::setFacing);
}

bool WowPlayer::ClickToMove(CtmType const ctm_type,
                            Guid const target_guid,
                            XYZ const& target_pos,
                            float const precision)
{
  auto const ctm_fn = reinterpret_cast<CGPlayer_C__ClickToMove_Fn>(
    offsets::Functions::CGPlayer_C__ClickToMove);

  SetFacing(target_pos);

  return (ctm_fn)(base_ptr, static_cast<uint32_t>(ctm_type), &target_guid,
                  &target_pos, precision);
}
}