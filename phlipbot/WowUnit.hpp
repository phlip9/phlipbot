#pragma once

#include <vector>

#include "WowObject.hpp"

namespace phlipbot
{
struct WowUnit : WowObject {
public:
  explicit WowUnit(Guid const guid, uintptr_t const base_ptr)
    : WowObject(guid, base_ptr)
  {
  }
  WowUnit(const WowUnit& obj) = default;
  virtual ~WowUnit() = default;

  inline CMovementData* GetMovement() const
  {
    uintptr_t move_data_ptr =
      base_ptr +
      static_cast<ptrdiff_t>(offsets::Descriptors::Unit__CMovementData);

    return reinterpret_cast<CMovementData*>(move_data_ptr);
  }

  inline Guid GetSummonedByGuid() const
  {
    return GetDescriptor<Guid>(offsets::Descriptors::SummonedByGuid);
  }

  inline uint32_t GetNpcId() const
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::NpcId);
  }

  inline uint32_t GetFactionId() const
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::FactionId);
  }

  inline uint32_t GetDynamicFlags() const
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::DynamicFlags);
  }

  inline bool GetIsTagged() const
  {
    uint32_t const dynamic_flags = GetDynamicFlags();
    return (dynamic_flags & DynamicFlags::tagged) != 0;
  }

  inline bool GetIsLootable() const
  {
    uint32_t const dynamic_flags = GetDynamicFlags();
    return (dynamic_flags & DynamicFlags::isDeadMobMine) != 0;
  }

  // TODO(phlip9): debug health, mana, rage, targetguid

  inline uint32_t GetHealth() const
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::Health);
  }

  inline uint32_t GetMaxHealth() const
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::MaxHealth);
  }

  inline float GetHealthPercent() const
  {
    float const health = static_cast<float>(GetHealth());
    float const max_health = static_cast<float>(GetMaxHealth());
    return (health / max_health) * 100;
  }

  inline uint32_t GetMana() const
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::Mana);
  }

  inline uint32_t GetMaxMana() const
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::MaxMana);
  }

  inline float GetManaPercent() const
  {
    float const mana = static_cast<float>(GetMana());
    float const max_mana = static_cast<float>(GetMaxMana());
    return (mana / max_mana) * 100;
  }

  inline uint32_t GetRage() const
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::Rage) / 10;
  }

  inline Guid GetTargetGuid() const
  {
    return GetDescriptor<Guid>(offsets::Descriptors::TargetGuid);
  }

  inline uint32_t GetCastingSpellId() const
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::CastingSpellId);
  }

  inline uint32_t GetChannelingSpellId() const
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::ChannelingSpellId);
  }

  // TODO(phlip9): is critter
  // bool GetIsCritter();

  virtual std::string GetName() const override;
  virtual ObjectType GetObjectType() const override;
  virtual void PrintToStream(std::ostream& os) const override;

  std::vector<uint32_t> GetBuffIds() const;
  std::vector<uint32_t> GetDebuffIds() const;
};
}
