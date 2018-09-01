#include "PhlipBot.hpp"

#include <hadesmem/detail/smart_handle.hpp>
#include <hadesmem/detail/trace.hpp>

// TODO(phlip9): use boost::sml or some state machine library to handle bot
//               state transitions?
// TODO(phlip9): use more robust way of detecting whether we're ingame or not
// TODO(phlip9): anti-afk
// TODO(phlip9): flexible configuration?

using std::chrono::duration;
using std::chrono::duration_cast;

using hadesmem::detail::SmartComHandle;

using hadesmem::ErrorCodeWinHr;
using hadesmem::ErrorString;

namespace
{
void SetDefaultRenderState(HWND const hwnd, IDirect3DDevice9* device)
{
  device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
  device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
  device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
  device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
  device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
  device->SetRenderState(D3DRS_LASTPIXEL, TRUE);
  device->SetRenderState(D3DRS_FOGENABLE, FALSE);
  device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
  device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);
  device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
  device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
  device->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, FALSE);

  device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
  device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
  device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
  device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
  device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
  device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
  device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU);
  device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS,
                               D3DTTFF_DISABLE);

  device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
  device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
  device->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
  device->SetSamplerState(0, D3DSAMP_BORDERCOLOR, 0);
  device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
  device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
  device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
  device->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, 0);
  device->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
  device->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 1);
  device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);
  device->SetSamplerState(0, D3DSAMP_ELEMENTINDEX, 0);
  device->SetSamplerState(0, D3DSAMP_DMAPOFFSET, 0);

  device->SetVertexShader(nullptr);
  device->SetPixelShader(nullptr);

  RECT rect = {};
  ::GetClientRect(hwnd, &rect);

  D3DVIEWPORT9 vp = {};
  vp.Width = rect.right - rect.left;
  vp.Height = rect.bottom - rect.top;
  vp.MaxZ = 1;
  device->SetViewport(&vp);
}
}


namespace phlipbot
{
PhlipBot::PhlipBot() noexcept
  : is_render_initialized(false),
    prev_frame_time(steady_clock::now()),
    input(),
    player_controller(),
    gui(player_controller)
{
}

void PhlipBot::Init() { HADESMEM_DETAIL_TRACE_A("initializing bot"); }

void PhlipBot::Update()
{
  float const dt = UpdateClock();

  auto& objmgr = ObjectManager::Get();
  if (objmgr.IsInGame()) {
    objmgr.EnumVisibleObjects();

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

  // Save the current device state so we can restore it after our we render
  IDirect3DStateBlock9* state_block = nullptr;
  auto const create_sb_hr = device->CreateStateBlock(D3DSBT_ALL, &state_block);
  if (FAILED(create_sb_hr)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"device->CreateStateBlock failed"}
                        << ErrorCodeWinHr{create_sb_hr});
  }
  SmartComHandle state_block_cleanup{state_block};

  // Set up needed to render GUI
  SetDefaultRenderState(hwnd, device);

  // Render our GUI
  gui.Render();

  // Restore the original device state
  auto const apply_sb_hr = state_block->Apply();
  if (FAILED(apply_sb_hr)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(hadesmem::Error{}
                                    << ErrorString{"state_block->Apply failed"}
                                    << ErrorCodeWinHr{apply_sb_hr});
  }
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