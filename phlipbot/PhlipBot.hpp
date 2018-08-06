#pragma once

#include <Shlwapi.h>
#include <d3d9.h>

#include "Gui.hpp"
#include "Input.hpp"

namespace phlipbot
{
struct PhlipBot final {
public:
  PhlipBot() = default;
  ~PhlipBot() = default;
  PhlipBot(const PhlipBot&) = delete;
  PhlipBot& operator=(const PhlipBot&) = delete;

  void Init();
  void Update();
  void Shutdown(HWND const hwnd);

  void InitRender(HWND const hwnd, IDirect3DDevice9* device);
  void Render(HWND const hwnd, IDirect3DDevice9* device);
  void ResetRender(HWND const hwnd);

  bool is_render_initialized = false;
  Input input;
  Gui gui;
};
}