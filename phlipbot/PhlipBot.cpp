#include "PhlipBot.hpp"

#include <hadesmem/detail/trace.hpp>

// TODO(phlip9): use boost::sml or some state machine library to handle bot
//               state transitions?
// TODO(phlip9): use more robust way of detecting if we're ingame
// TODO(phlip9): anti-afk
// TODO(phlip9): configuration file? e.g., mmaps directory, PID values, etc

using std::chrono::duration;
using std::chrono::duration_cast;

namespace
{
D3DVIEWPORT9 BuildViewport(HWND const hwnd)
{
  RECT rect = {};
  ::GetClientRect(hwnd, &rect);

  D3DVIEWPORT9 vp = {};
  vp.Width = rect.right - rect.left;
  vp.Height = rect.bottom - rect.top;
  vp.MinZ = 0;
  vp.MaxZ = 1;
  return vp;
}
}

namespace phlipbot
{
PhlipBot::PhlipBot() noexcept
  : is_render_initialized(false),
    prev_frame_time(steady_clock::now()),
    objmgr(),
    input(),
    player_controller(objmgr),
    mmap_mgr("C:\\MaNGOS\\data\\__mmaps"),
    player_nav(objmgr, player_controller, mmap_mgr),
    gui(objmgr, player_controller, player_nav)
{
}

void PhlipBot::Init() { HADESMEM_DETAIL_TRACE_A("initializing bot"); }

void PhlipBot::Update()
{
  float const dt = UpdateClock();

  if (objmgr.IsInGame()) {
    objmgr.EnumVisibleObjects();

    player_nav.Update();
    player_controller.Update(dt);
  }
}

void PhlipBot::Shutdown(HWND const hwnd)
{
  HADESMEM_DETAIL_TRACE_FORMAT_A("hwd : [%p], is_render_initialized: [%d]",
                                 hwnd, is_render_initialized);
  HADESMEM_DETAIL_ASSERT(is_render_initialized);

  if (input.is_hooked) {
    input.UnhookInput(hwnd);
  }

  if (gui.is_initialized) {
    gui.Shutdown();
  }

  is_render_initialized = false;
}

void PhlipBot::InitRender(HWND const hwnd, IDirect3DDevice9* device)
{
  HADESMEM_DETAIL_TRACE_FORMAT_A(
    "hwd: [%p], device: [%p], is_render_initialized: [%d]", hwnd, device,
    is_render_initialized);
  HADESMEM_DETAIL_ASSERT(!is_render_initialized);

  input.HookInput(hwnd);
  gui.Init(hwnd, device);

  is_render_initialized = true;
}

void PhlipBot::Render(HWND const hwnd, IDirect3DDevice9* device)
{
  HADESMEM_DETAIL_ASSERT(is_render_initialized);

  // test cooperative level
  auto const coop_level = device->TestCooperativeLevel();
  if (FAILED(coop_level)) {
    HADESMEM_DETAIL_TRACE_A(
      "device->TestCooperativeLevel failed, skipping this frame");
    return;
  }

  // Set up needed to render GUI
  auto vp = BuildViewport(hwnd);

  // Render our GUI
  gui.Render(device, vp);
}

void PhlipBot::ResetRender(HWND const hwnd)
{
  HADESMEM_DETAIL_TRACE_FORMAT_A("hwd: [%p], is_render_initialized: [%d]", hwnd,
                                 is_render_initialized);

  // Uninitialize ImGui on device reset
  if (is_render_initialized) {
    input.UnhookInput(hwnd);
    gui.Reset();
  }

  is_render_initialized = false;
}

float PhlipBot::UpdateClock()
{
  auto const frame_time = steady_clock::now();
  auto const frame_duration_ns = frame_time - prev_frame_time;
  auto const frame_duration_s =
    duration_cast<duration<float>>(frame_duration_ns);
  float const dt = frame_duration_s.count();
  prev_frame_time = frame_time;
  return dt;
}
}