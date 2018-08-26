#pragma once

#include <Shlwapi.h>
#include <d3d9.h>
#include <stdint.h>

namespace phlipbot
{
// TODO(phlip9): Refactor out once we implement an input message queue
bool& GetGuiIsVisible();
void SetGuiIsVisible(bool val);
void ToggleGuiIsVisible();

struct Gui final {
public:
  Gui() = default;
  ~Gui() = default;
  Gui(const Gui&) = delete;
  Gui& operator=(const Gui&) = delete;

  void Init(HWND const hwnd, IDirect3DDevice9* device);
  void Render();
  void Reset();
  void Shutdown();

  bool is_initialized{false};

  float player_facing = 0.0f;
  float ctm_precision = 2.0f;
  bool ctm_toggle = false;
  float ctm_dx = 0.0f;
  float ctm_dy = 0.0f;
};
}