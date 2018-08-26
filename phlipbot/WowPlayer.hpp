#pragma once

#include "wow_constants.hpp"

#include "WowUnit.hpp"

namespace phlipbot
{
struct WowPlayer : WowUnit {
public:
  explicit WowPlayer(types::Guid const guid, uintptr_t const base_ptr)
    : WowUnit(guid, base_ptr)
  {
  }
  WowPlayer(const WowPlayer& obj) = default;
  virtual ~WowPlayer() = default;

  virtual std::string GetName() const override;
  virtual phlipbot::types::ObjectType GetObjectType() const override;
  virtual void PrintToStream(std::ostream& os) const override;

  void SendUpdateMovement(uint32_t timestamp, types::MovementOpCode opcode);

  void SetFacing(float facing_radians);
  void SetFacing(types::XYZ const& target_pos);

  bool ClickToMove(types::CtmType const ctm_type,
                   types::Guid const target_guid,
                   types::XYZ const& target_pos,
                   float const precision = 2.0f);
};
}
