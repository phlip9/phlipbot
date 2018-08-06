#pragma once

#include <Windows.h>
#include <d3d9.h>
#include <stdint.h>

#include <hadesmem/process.hpp>

#include "detour_helpers.hpp"

namespace phlipbot
{
using IDirect3DDevice9_EndScene_Fn = HRESULT(WINAPI*)(IDirect3DDevice9* device);
using IDirect3DDevice9_Reset_Fn = HRESULT(WINAPI*)(IDirect3DDevice9* device,
                                                   D3DPRESENT_PARAMETERS* pp);

struct d3d9_offsets {
  uintptr_t end_scene;
  uintptr_t reset;
};

std::unique_ptr<hadesmem::PatchDetour<phlipbot::IDirect3DDevice9_EndScene_Fn>>&
GetIDirect3DDevice9EndSceneDetour() noexcept;

std::unique_ptr<hadesmem::PatchDetour<phlipbot::IDirect3DDevice9_Reset_Fn>>&
GetIDirect3DDevice9ResetDetour() noexcept;

d3d9_offsets GetD3D9Offsets(IDirect3DDevice9Ex* device_ex);

IDirect3DDevice9Ex*
GetD3D9Device(hadesmem::Process const& process, HWND const wnd);

template <typename EndSceneFn, typename ResetFn>
inline void DetourD3D9(hadesmem::Process const& process,
                       HWND const& hwnd,
                       EndSceneFn endscene_fn,
                       ResetFn reset_fn)
{
  IDirect3DDevice9Ex* device_ex = GetD3D9Device(process, hwnd);
  d3d9_offsets offsets = GetD3D9Offsets(device_ex);

  DetourFn(process, "IDirect3DDevice9::EndScene",
           GetIDirect3DDevice9EndSceneDetour(),
           reinterpret_cast<IDirect3DDevice9_EndScene_Fn>(offsets.end_scene),
           endscene_fn);

  DetourFn(process, "IDirect3DDevice9::Reset", GetIDirect3DDevice9ResetDetour(),
           reinterpret_cast<IDirect3DDevice9_Reset_Fn>(offsets.reset),
           reset_fn);
}

inline bool IsD3D9Detoured()
{
  // check that both detours are set
  return GetIDirect3DDevice9EndSceneDetour() &&
         GetIDirect3DDevice9ResetDetour();
}

inline void UndetourD3D9()
{
  UndetourFn("IDirect3DDevice9::EndScene", GetIDirect3DDevice9EndSceneDetour());
  UndetourFn("IDirect3DDevice9::Reset", GetIDirect3DDevice9ResetDetour());
}

template <typename DeviceT>
inline HWND GetWindowFromDevice(DeviceT* const device)
{
  D3DDEVICE_CREATION_PARAMETERS cp;
  auto const get_cp_hr = device->GetCreationParameters(&cp);
  if (FAILED(get_cp_hr)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error() << hadesmem::ErrorString("GetCreationParameters failed")
                        << hadesmem::ErrorCodeWinHr{get_cp_hr});
  }
  return cp.hFocusWindow;
}
}