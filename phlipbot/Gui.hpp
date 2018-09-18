#pragma once

#include <Shlwapi.h>
#include <d3d9.h>
#include <stdint.h>

#include "ObjectManager.hpp"
#include "PlayerController.hpp"
#include "navigation/PlayerNavigator.hpp"
#include "wow_constants.hpp"

namespace phlipbot
{
// TODO(phlip9): Refactor out once we implement an input message queue
bool& GetGuiIsVisible();
void SetGuiIsVisible(bool val);
void ToggleGuiIsVisible();

struct Gui final {
public:
  explicit Gui(ObjectManager& objmgr,
               PlayerController& pc,
               PlayerNavigator& pn) noexcept;
  ~Gui() = default;
  Gui(const Gui&) = delete;
  Gui& operator=(const Gui&) = delete;

  void Init(HWND const hwnd, IDirect3DDevice9* device);
  void SetRenderState(IDirect3DDevice9* device, D3DVIEWPORT9 const& vp);
  void Render(IDirect3DDevice9* device, D3DVIEWPORT9 const& vp);
  void RenderImGui();
  void Reset();
  void Shutdown();

  enum class FacingType : int { Direction, Position, Object };

  ObjectManager& objmgr;
  PlayerController& player_controller;
  PlayerNavigator& player_nav;

  bool is_initialized{false};

  float ctm_precision{2.0f};
  bool ctm_toggle{false};
  float ctm_dx{0.0f};
  float ctm_dy{0.0f};

  int facing_type = int(FacingType::Direction);
  float facing_direction{0.0f};
  vec3 facing_position{};
  Guid facing_guid{};

  bool player_controller_enabled{false};

  vec3 nav_destination{};
  bool player_nav_enabled{false};

  uint32_t input_flags{0};
  bool control_toggle{false};
};
}