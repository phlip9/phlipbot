#include "dxhook.hpp"

#include <hadesmem/detail/smart_handle.hpp>
#include <hadesmem/detail/trace.hpp>
#include <hadesmem/error.hpp>
#include <hadesmem/module.hpp>

using std::unique_ptr;

using hadesmem::ErrorCodeWinHr;
using hadesmem::ErrorCodeWinLast;
using hadesmem::ErrorString;
using hadesmem::Module;
using hadesmem::Process;
using hadesmem::detail::SmartComHandle;

namespace phlipbot
{
unique_ptr<EndSceneDetour>& GetIDirect3DDevice9EndSceneDetour() noexcept
{
  static unique_ptr<EndSceneDetour> detour;
  return detour;
}

unique_ptr<ResetDetour>& GetIDirect3DDevice9ResetDetour() noexcept
{
  static unique_ptr<ResetDetour> detour;
  return detour;
}

d3d9_offsets GetD3D9Offsets(IDirect3DDevice9Ex* device_ex)
{
  auto const end_scene_fn = (*reinterpret_cast<void***>(device_ex))[42];
  auto const reset_fn = (*reinterpret_cast<void***>(device_ex))[16];

  HADESMEM_DETAIL_TRACE_FORMAT_A("IDirect3D9Ex::EndScene: %p", end_scene_fn);
  HADESMEM_DETAIL_TRACE_FORMAT_A("IDirect3D9Ex::Reset: %p", reset_fn);

  d3d9_offsets offsets{};
  offsets.end_scene = reinterpret_cast<std::uintptr_t>(end_scene_fn);
  offsets.reset = reinterpret_cast<std::uintptr_t>(reset_fn);
  return offsets;
}

IDirect3DDevice9Ex* GetD3D9Device(Process const& process, HWND const wnd)
{
  // get d3d9.dll module
  Module const d3d9_mod{process, L"d3d9.dll"};

  // get the Direct3DCreate9Ex function
  auto const direct3d_create_9_ex =
    reinterpret_cast<decltype(&Direct3DCreate9Ex)>(
      ::GetProcAddress(d3d9_mod.GetHandle(), "Direct3DCreate9Ex"));
  if (!direct3d_create_9_ex) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{}
      << ErrorString{"GetProcAddress for Direct3DCreate9Ex failed"}
      << ErrorCodeWinLast{::GetLastError()});
  }

  // get the IDirect3D9Ex interface impl
  IDirect3D9Ex* d3d9_ex = nullptr;
  auto const create_d3d9_ex_hr =
    direct3d_create_9_ex(D3D_SDK_VERSION, &d3d9_ex);
  if (FAILED(create_d3d9_ex_hr)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(hadesmem::Error{}
                                    << ErrorString{"Direct3DCreate9Ex failed"}
                                    << ErrorCodeWinHr{create_d3d9_ex_hr});
  }

  SmartComHandle smart_d3d9_ex{d3d9_ex};

  D3DPRESENT_PARAMETERS pp{};
  pp.Windowed = TRUE;
  pp.SwapEffect = D3DSWAPEFFECT_FLIP;
  pp.BackBufferFormat = D3DFMT_A8R8G8B8;
  pp.BackBufferWidth = 2;
  pp.BackBufferHeight = 2;
  pp.BackBufferCount = 1;
  pp.hDeviceWindow = wnd;
  pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

  // get the device
  IDirect3DDevice9Ex* device_ex = nullptr;
  auto const create_device_hr = d3d9_ex->CreateDeviceEx(
    D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd,
    D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES, &pp,
    nullptr, &device_ex);
  if (FAILED(create_device_hr)) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << ErrorString{"IDirect3D9Ex::CreateDeviceEx failed"}
                        << ErrorCodeWinHr{create_device_hr});
  }

  return device_ex;
}
}