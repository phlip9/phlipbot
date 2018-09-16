#pragma once

#include <Shlwapi.h>
#include <chrono>
#include <d3d9.h>

#include "Gui.hpp"
#include "Input.hpp"
#include "ObjectManager.hpp"
#include "PlayerController.hpp"
#include "navigation/MoveMap.hpp"
#include "navigation/PlayerNavigator.hpp"


namespace phlipbot
{
struct PhlipBot final {
  using steady_clock = std::chrono::steady_clock;

public:
  explicit PhlipBot() noexcept;
  ~PhlipBot() = default;
  PhlipBot(const PhlipBot&) = delete;
  PhlipBot& operator=(const PhlipBot&) = delete;

  void Init();
  void Update();
  void Shutdown(HWND const hwnd);

  void InitRender(HWND const hwnd, IDirect3DDevice9* device);
  void Render(HWND const hwnd, IDirect3DDevice9* device);
  void ResetRender(HWND const hwnd);

  float UpdateClock();

  bool is_render_initialized;
  steady_clock::time_point prev_frame_time;

  ObjectManager objmgr;
  Input input;
  PlayerController player_controller;
  MMapManager mmap_mgr;
  PlayerNavigator player_nav;
  Gui gui;
};
}