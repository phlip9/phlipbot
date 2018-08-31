#pragma once

#include "WowUnit.hpp"

namespace phlipbot
{
struct WowPlayer : WowUnit {
public:
  explicit WowPlayer(Guid const guid, uintptr_t const base_ptr)
    : WowUnit(guid, base_ptr)
  {
  }
  WowPlayer(const WowPlayer& obj) = default;
  virtual ~WowPlayer() = default;

  virtual std::string GetName() const override;
  virtual ObjectType GetObjectType() const override;
  virtual void PrintToStream(std::ostream& os) const override;

  void SendUpdateMovement(uint32_t timestamp, MovementOpCode opcode);

  void SetFacing(float facing_radians);
  void SetFacing(Vec3 const& target_pos);

  bool ClickToMove(CtmType const ctm_type,
                   Guid const target_guid,
                   Vec3 const& target_pos,
                   float const precision = 2.0f);

  uint32_t SetControlBits(uint32_t flags, uint32_t timestamp);
  uint32_t UnsetControlBits(uint32_t flags, uint32_t timestamp);
};
}
