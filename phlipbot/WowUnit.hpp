#pragma once

#include <vector>

#include "WowObject.hpp"
#include "memory.hpp"
#include "wow_constants.hpp"

namespace phlipbot
{
struct WowUnit : WowObject {
public:
  explicit WowUnit(types::Guid const guid, uintptr_t const base_ptr)
    : WowObject(guid, base_ptr)
  {
  }
  WowUnit(const WowUnit& obj) = default;
  virtual ~WowUnit(){};

  inline types::XYZ GetPosition()
  {
    return GetDescriptor<types::XYZ>(offsets::Descriptors::UnitPos);
  }

  inline types::Guid GetSummonedByGuid()
  {
    return GetDescriptor<types::Guid>(offsets::Descriptors::SummonedByGuid);
  }

  inline uint32_t GetNpcId()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::NpcId);
  }

  inline uint32_t GetFactionId()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::FactionId);
  }

  inline uint32_t GetMovementFlags()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::MovementFlags);
  }

  inline uint32_t GetDynamicFlags()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::DynamicFlags);
  }

  inline bool GetIsTagged()
  {
    uint32_t const dynamic_flags = GetDynamicFlags();
    return (dynamic_flags & types::DynamicFlags::tagged) != 0;
  }

  inline bool GetIsLootable()
  {
    uint32_t const dynamic_flags = GetDynamicFlags();
    return (dynamic_flags & types::DynamicFlags::isDeadMobMine) != 0;
  }

  inline uint32_t GetHealth()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::Health);
  }

  inline uint32_t GetMaxHealth()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::MaxHealth);
  }

  inline float GetHealthPercent()
  {
    float const health = static_cast<float>(GetHealth());
    float const max_health = static_cast<float>(GetMaxHealth());
    return (health / max_health) * 100;
  }

  inline uint32_t GetMana()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::Mana);
  }

  inline uint32_t GetMaxMana()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::MaxMana);
  }

  inline float GetManaPercent()
  {
    float const mana = static_cast<float>(GetMana());
    float const max_mana = static_cast<float>(GetMaxMana());
    return (mana / max_mana) * 100;
  }

  inline uint32_t GetRage()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::Rage) / 10;
  }

  inline types::Guid GetTargetGuid()
  {
    return GetDescriptor<types::Guid>(offsets::Descriptors::TargetGuid);
  }

  inline uint32_t GetCastingSpellId()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::CastingSpellId);
  }

  inline uint32_t GetChannelingSpellId()
  {
    return GetDescriptor<uint32_t>(offsets::Descriptors::ChannelingSpellId);
  }

  // TODO(phlip9): is critter
  // bool GetIsCritter();

  virtual std::string GetName();

  std::vector<uint32_t> GetBuffIds();
  std::vector<uint32_t> GetDebuffIds();

  phlipbot::types::ObjectType const obj_type{phlipbot::types::ObjectType::UNIT};
};
}
