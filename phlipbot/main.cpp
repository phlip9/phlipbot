#include <Shlwapi.h>
//#include <condition_variable>
#include <chrono>
#include <d3d9.h>
#include <thread>

#include <boost/optional.hpp>

#include <hadesmem/detail/smart_handle.hpp>
#include <hadesmem/detail/trace.hpp>
#include <hadesmem/error.hpp>
#include <hadesmem/module.hpp>
#include <hadesmem/module_list.hpp>
#include <hadesmem/process.hpp>

#include "PhlipBot.hpp"
#include "dxhook.hpp"

// TODO(phlip9): use boost::sml or some state machine library to handle
//               injection state transitions?

void LogModules()
{
  hadesmem::Process const process(::GetCurrentProcessId());

  hadesmem::ModuleList const module_list(process);

  HADESMEM_DETAIL_TRACE_A("WoW.exe dll modules:");
  for (auto const& module : module_list) {
    HADESMEM_DETAIL_TRACE_FORMAT_W(L"    %s", module.GetName().c_str());
  }
}

void LogWindowTitle(HWND const wnd)
{
  char buf[256];
  if (!GetWindowTextA(wnd, static_cast<LPSTR>(buf), sizeof(buf))) {
    HADESMEM_DETAIL_THROW_EXCEPTION(
      hadesmem::Error{} << hadesmem::ErrorString{"GetWindowTextA failed"}
                        << hadesmem::ErrorCodeWinLast{::GetLastError()});
  }
  HADESMEM_DETAIL_TRACE_FORMAT_A("window title = %s", buf);
}

// state passed to EnumWindowsCallback
struct enum_windows_aux {
  DWORD target_proc_id;
  HWND target_window_handle;
};

// GetMainWindow callback
BOOL CALLBACK EnumWindowsCallback(HWND const handle, LPARAM const lparam)
{
  enum_windows_aux& aux_data = *reinterpret_cast<enum_windows_aux*>(lparam);

  // query the owning process
  DWORD process_id;
  GetWindowThreadProcessId(handle, &process_id);

  // continue if this window doesn't belong to the target process
  if (process_id != aux_data.target_proc_id) {
    return TRUE;
  }

  // ensure not a child window
  if (GetWindow(handle, GW_OWNER) != nullptr) {
    return TRUE;
  }

  // found the primary window
  aux_data.target_window_handle = handle;
  return FALSE;
}

// find the root window of a process
boost::optional<HWND> FindMainWindow(hadesmem::Process const& process)
{
  enum_windows_aux aux_data;
  aux_data.target_proc_id = process.GetId();
  aux_data.target_window_handle = nullptr;

  auto aux_ptr = reinterpret_cast<LPARAM>(&aux_data);
  EnumWindows(EnumWindowsCallback, aux_ptr);

  if (aux_data.target_window_handle) {
    return aux_data.target_window_handle;
  } else {
    return boost::none;
  }
}

// TODO(phlip9): Do we need synchronization around these? Unload gets called in
//               a separate thread...

// Reference to the PhlipBot instance. Should only be used in
// the directx hooks to manage bot lifecycle.
std::unique_ptr<phlipbot::PhlipBot>& GetBotInstance() noexcept
{
  static std::unique_ptr<phlipbot::PhlipBot> bot;
  return bot;
}

// Flag to signal when Unload() has been called, so the bot can cleanly
// shutdown on the next render frame.
bool& GetIsShuttingDown() noexcept
{
  static bool is_shutting_down;
  return is_shutting_down;
}
void SetIsShuttingDown(bool val) noexcept { GetIsShuttingDown() = val; }

// Flag for the bot to signal when it has completely shutdown. Unload() spins
// on this flag.
// std::condition_variable& GetHasShutdownCV()
//{
//  static std::condition_variable has_shutdown_cv;
//  return has_shutdown_cv;
//}
bool& GetHasShutdown() noexcept
{
  static bool has_shutdown;
  return has_shutdown;
}
void SetHasShutdown(bool val) noexcept { GetHasShutdown() = val; }

// Called every end scene. Handles bot lifecycle management and calling into
// the bot main loop.
extern "C" HRESULT WINAPI IDirect3DDevice9_EndScene_Detour(
  hadesmem::PatchDetourBase* detour, IDirect3DDevice9* device)
{
  try {
    // Do nothing if we've already shutdown
    if (!GetHasShutdown()) {

      // get the current window from the directx device
      HWND const hwnd = phlipbot::GetWindowFromDevice(device);

      // catch shutdown signal
      if (GetIsShuttingDown()) {
        HADESMEM_DETAIL_TRACE_A("Received Shutdown flag");

        // ... shutdown stuff
        auto& bot = GetBotInstance();
        if (bot) {
          bot->Shutdown(hwnd);
          bot = nullptr;
        }

        // signal Unload that the bot has shutdown
        SetIsShuttingDown(false);
        SetHasShutdown(true);

      } else {
        // Run the main loop and render loop

        auto& bot = GetBotInstance();
        if (!bot) {
          // initialize bot
          bot.reset(new phlipbot::PhlipBot);
          bot->Init();
        }

        // call the main bot loop
        bot->Update();

        if (!bot->is_render_initialized) {
          bot->InitRender(hwnd, device);
        }

        if (bot->is_render_initialized) {
          bot->Render(hwnd, device);
        }
      }
    }
  } catch (...) {
    HADESMEM_DETAIL_TRACE_A(
      boost::current_exception_diagnostic_information().c_str());

    SetIsShuttingDown(false);
    SetHasShutdown(true);
  }

  // call the original end scene
  auto const end_scene =
    detour->GetTrampolineT<phlipbot::IDirect3DDevice9_EndScene_Fn>();
  auto ret = end_scene(device);

  return ret;
}

// Hook device reset so we can reset our internal renderer on e.g. window
// resize.
extern "C" HRESULT WINAPI
IDirect3DDevice9_Reset_Detour(hadesmem::PatchDetourBase* detour,
                              IDirect3DDevice9* device,
                              D3DPRESENT_PARAMETERS* pp)
{
  HADESMEM_DETAIL_TRACE_FORMAT_A("Args: [%p] [%p]", device, pp);

  // get the current window from the directx device
  HWND const hwnd = phlipbot::GetWindowFromDevice(device);

  // Reset the bot renderer if it's initialized
  auto& bot = GetBotInstance();
  if (bot && bot->is_render_initialized) {
    bot->ResetRender(hwnd);
  }

  // call the original device reset
  auto const reset =
    detour->GetTrampolineT<phlipbot::IDirect3DDevice9_Reset_Fn>();
  auto ret = reset(device, pp);

  HADESMEM_DETAIL_TRACE_FORMAT_A("Ret: [%ld]", ret);

  return ret;
}

// Load is called by the bot dll loader after the bot dll has been injected
// into the WoW.exe process.
extern "C" __declspec(dllexport) unsigned int Load()
{
  HADESMEM_DETAIL_TRACE_A("phlipbot dll Load() called.");

  // TODO(phlip9): prevent detours from initializing until load is complete

  SetIsShuttingDown(false);
  SetHasShutdown(false);

  hadesmem::Process const process{::GetCurrentProcessId()};

  // get the WoW.exe main window handle
  boost::optional<HWND> omain_hwnd = FindMainWindow(process);
  if (!omain_hwnd) {
    HADESMEM_DETAIL_TRACE_A("ERROR: Could not find main window handle");
    return EXIT_FAILURE;
  }
  HWND main_hwnd = *omain_hwnd;

  // log the main window title
  try {
    LogWindowTitle(main_hwnd);
  } catch (...) {
    return EXIT_FAILURE;
  }

  // hook d3d9 device reset and end scene
  try {
    phlipbot::DetourD3D9(process, main_hwnd, &IDirect3DDevice9_EndScene_Detour,
                         &IDirect3DDevice9_Reset_Detour);
  } catch (...) {
    HADESMEM_DETAIL_TRACE_A("ERROR: Failed to detour end scene and reset");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

// Unload is called when the loader wants to shutdown the bot and eject the dll
// from the WoW.exe process.
extern "C" __declspec(dllexport) unsigned int Unload()
{
  HADESMEM_DETAIL_TRACE_A("phlipbot dll Unload() called");

  // signal the main thread to begin shutting down
  SetIsShuttingDown(true);
  SetHasShutdown(false);

  bool is_detoured = phlipbot::IsD3D9Detoured();
  if (is_detoured) {
    // spin until the bot shuts down
    while (!GetHasShutdown()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    phlipbot::UndetourD3D9();
  }

  SetIsShuttingDown(false);
  SetHasShutdown(true);

  return EXIT_SUCCESS;
}

// Currently unused. See Load/Unload for bot lifecycle management.
BOOL APIENTRY DllMain(HMODULE /* hModule */,
                      DWORD ul_reason_for_call,
                      LPVOID /* lpReserved */
)
{
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    HADESMEM_DETAIL_TRACE_A("DLL_PROCESS_ATTACH");
    break;
  case DLL_PROCESS_DETACH:
    HADESMEM_DETAIL_TRACE_A("DLL_PROCESS_DETACH");
    break;
  case DLL_THREAD_ATTACH:
    HADESMEM_DETAIL_TRACE_A("DLL_THREAD_ATTACH");
    break;
  case DLL_THREAD_DETACH:
    HADESMEM_DETAIL_TRACE_A("DLL_THREAD_DETACH");
    break;
  }
  return TRUE;
}