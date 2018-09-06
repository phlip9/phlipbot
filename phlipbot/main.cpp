#include <Shlwapi.h>
#undef max // undefine max to remove boost/sml name collision
#include <chrono>
#include <d3d9.h>
#include <mutex>
#include <queue>
#include <thread>

#include <boost/optional.hpp>
#include <boost/sml.hpp>

#include <hadesmem/detail/smart_handle.hpp>
#include <hadesmem/detail/trace.hpp>
#include <hadesmem/error.hpp>
#include <hadesmem/module.hpp>
#include <hadesmem/module_list.hpp>
#include <hadesmem/process.hpp>

#include "PhlipBot.hpp"
#include "dxhook.hpp"

// TODO(phlip9): use mutex + cv to wait for main thread to signal unload
// TODO(phlip9): hook window close to ensure bot properly shuts down
// TODO(phlip9): timeout detouring and undetouring states?
// TODO(phlip9): handle unexpected transitions in the state machine
// TODO(phlip9): might need coarser locking around state machine. currently just
//               locking around process_event using the default thread_safety
//               policy, but should probably hold a lock on the whole state
//               machine when we check the current state to do something, like
//                 sm.is_state_and_lock_acquire(state<____>, [] {
//                   ... lambda runs if currently in state and also holds lock
//                 });
//               it's also not clear if sm.process_.push() is synchronized...

using std::unique_ptr;

using boost::none;
using boost::optional;

using hadesmem::ErrorCodeWinLast;
using hadesmem::ErrorString;
using hadesmem::ModuleList;
using hadesmem::PatchDetourBase;
using hadesmem::Process;

using phlipbot::DetourD3D9;
using phlipbot::GetWindowFromDevice;
using phlipbot::IDirect3DDevice9_EndScene_Fn;
using phlipbot::IDirect3DDevice9_Reset_Fn;
using phlipbot::IsD3D9Detoured;
using phlipbot::PhlipBot;
using phlipbot::UndetourD3D9;

namespace
{
void LogModules()
{
  Process const process(::GetCurrentProcessId());
  ModuleList const module_list(process);

  HADESMEM_DETAIL_TRACE_A("WoW.exe dll modules:");
  for (auto const& module : module_list) {
    HADESMEM_DETAIL_TRACE_FORMAT_W(L"    %s", module.GetName().c_str());
  }
}

void LogWindowTitle(HWND const wnd)
{
  char buf[256];
  if (!::GetWindowTextA(wnd, static_cast<LPSTR>(buf), sizeof(buf))) {
    HADESMEM_DETAIL_THROW_EXCEPTION(hadesmem::Error{}
                                    << ErrorString{"GetWindowTextA failed"}
                                    << ErrorCodeWinLast{::GetLastError()});
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
  ::GetWindowThreadProcessId(handle, &process_id);

  // continue if this window doesn't belong to the target process
  if (process_id != aux_data.target_proc_id) {
    return TRUE;
  }

  // ensure not a child window
  if (::GetWindow(handle, GW_OWNER) != nullptr) {
    return TRUE;
  }

  // found the primary window
  aux_data.target_window_handle = handle;
  return FALSE;
}

// find the root window of a process
optional<HWND> FindMainWindow(Process const& process)
{
  enum_windows_aux aux_data;
  aux_data.target_proc_id = process.GetId();
  aux_data.target_window_handle = nullptr;

  auto aux_ptr = reinterpret_cast<LPARAM>(&aux_data);
  ::EnumWindows(EnumWindowsCallback, aux_ptr);

  if (aux_data.target_window_handle) {
    return aux_data.target_window_handle;
  } else {
    return none;
  }
}

// TODO(phlip9): Do we need synchronization around these? Unload gets called in
//               a separate thread...

// Reference to the PhlipBot instance. Should only be used in
// the directx hooks to manage bot lifecycle.
unique_ptr<PhlipBot>& GetBotInstance() noexcept
{
  static unique_ptr<PhlipBot> bot;
  return bot;
}
}

// The state maching for detouring the EndScene/Reset functions
namespace detour_state_machine
{
namespace sml = boost::sml;

// Forward Declarations
struct debug_logger;
struct DetourStateMachine;

using LoggerPolicyT = sml::logger<debug_logger>;
using ThreadSafePolicyT = sml::thread_safe<std::recursive_mutex>;
using QueuePolicyT = sml::process_queue<std::queue>;
using StateMachineT = boost::sml::
  sm<DetourStateMachine, LoggerPolicyT, ThreadSafePolicyT, QueuePolicyT>;

StateMachineT& GetDetourStateMachine() noexcept;

// states

// clang-format off
struct detoured {};
struct detouring {};
struct shutting_down {};
struct undetoured {};
struct undetouring {};
struct error_handling {};
// clang-format on

// events

// clang-format off
struct dll_load {};
struct dll_unload {};
struct has_shutdown {};
struct has_undetoured {};
struct detour_success {};
struct detour_fail {};
struct detoured_error {};
// clang-format on

// actions

auto const undetour = []() {
  if (IsD3D9Detoured()) {
    UndetourD3D9();
  }
};

auto const detour =
  [](auto const& /* event */, auto& sm, auto& /* deps */, auto& /* subs */) {
    Process const proc{::GetCurrentProcessId()};

    // get the WoW.exe main window handle
    auto const omain_hwnd = FindMainWindow(proc);
    if (!omain_hwnd) {
      HADESMEM_DETAIL_TRACE_A("ERROR: Could not find main window handle");
      sm.process_.push(detour_fail{});
      return;
    }

    // log the main window title
    try {
      LogWindowTitle(omain_hwnd.value());
    } catch (...) {
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "Error: Failed to log window title: %s",
        boost::current_exception_diagnostic_information().c_str());
      sm.process_.push(detour_fail{});
      return;
    }

    // hook d3d9 device reset and end scene
    try {
      DetourD3D9(proc, *omain_hwnd, &IDirect3DDevice9_EndScene_Detour,
                 &IDirect3DDevice9_Reset_Detour);
    } catch (...) {
      HADESMEM_DETAIL_TRACE_FORMAT_A(
        "ERROR: Failed to detour end scene and reset: %s",
        boost::current_exception_diagnostic_information().c_str());
      sm.process_.push(detour_fail{});
      return;
    }

    sm.process_.push(detour_success{});
  };

// auto const log_unexpected =
//  [](auto const& ev, auto& /* sm */, auto& /* deps */, auto& /* subs */) {
//    HADESMEM_DETAIL_TRACE_FORMAT_A("Warn: [unexpected_event] %s",
//                                   typeid(ev).name());
//  };

// state machine

// defines the transition table for the detour state machine
//
//                             +----------+
//                +------------> detoured +--------------+
//                |            +---------++              |
//                |                      |               | dll_unload
// detour_success |                      |               |
//                |                      |       +-------v-------+
//                |                      |       | shutting_down |
//                |                      |       +-------+-------+
//          +-----+-----+                |               |
//          | detouring |            detoured_error      | has_shutdown
//          +-----^--+--+                |               |
//                |  |                   |        +------v------+
//                |  | detour_fail       +--------> undetouring |
//                |  |                            +------+------+
//       dll_load |  +-----------+                       |
//                |              |                       | has_undetoured
//                |           +--v---------+             |
//                +-----------+ undetoured <-------------+
//                            +------------+
//
struct DetourStateMachine {
  auto operator()() const noexcept
  {
    using namespace boost::sml;

    // clang-format off
    return make_transition_table(
      state<detouring>     <= *state<undetoured>     + event<dll_load>,
      state<undetoured>    <=  state<undetoured>     + event<dll_unload>,
                               state<detouring>      + on_entry<_> / detour,
      state<detoured>      <=  state<detouring>      + event<detour_success>,
      state<undetouring>   <=  state<detouring>      + event<detour_fail>,
                               state<detouring>      + exception<_> / process(detour_fail{}),
      state<shutting_down> <=  state<detoured>       + event<dll_unload>,
      state<undetouring>   <=  state<detoured>       + event<detoured_error>,
      state<undetouring>   <=  state<shutting_down>  + event<has_shutdown>,
      state<undetoured>    <=  state<undetouring>    + event<has_undetoured>
    );
    // clang-format on
  }
};

struct debug_logger {
  template <class SM, class TEvent>
  void log_process_event(const TEvent&)
  {
    HADESMEM_DETAIL_TRACE_FORMAT_A("[%s][process_event] %s",
                                   sml::aux::get_type_name<SM>(),
                                   sml::aux::get_type_name<TEvent>());
  }

  template <class SM, class TGuard, class TEvent>
  void log_guard(const TGuard&, const TEvent&, bool result)
  {
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "[%s][guard] %s %s %s", sml::aux::get_type_name<SM>(),
      sml::aux::get_type_name<TGuard>(), sml::aux::get_type_name<TEvent>(),
      (result ? "[OK]" : "[Reject]"));
  }

  template <class SM, class TAction, class TEvent>
  void log_action(const TAction&, const TEvent&)
  {
    HADESMEM_DETAIL_TRACE_FORMAT_A(
      "[%s][action] %s %s", sml::aux::get_type_name<SM>(),
      sml::aux::get_type_name<TAction>(), sml::aux::get_type_name<TEvent>());
  }

  template <class SM, class TSrcState, class TDstState>
  void log_state_change(const TSrcState& src, const TDstState& dst)
  {
    HADESMEM_DETAIL_TRACE_FORMAT_A("[%s][transition] %s -> %s",
                                   sml::aux::get_type_name<SM>(), src.c_str(),
                                   dst.c_str());
  }
};

StateMachineT& GetDetourStateMachine() noexcept
{
  static debug_logger logger;
  static StateMachineT sm{logger};
  return sm;
}
}


// Called every end scene. Handles bot lifecycle management and calling into
// the bot main loop.
extern "C" HRESULT WINAPI IDirect3DDevice9_EndScene_Detour(
  PatchDetourBase* detour, IDirect3DDevice9* device)
{
  using namespace detour_state_machine;
  using namespace boost::sml;

  auto& sm = GetDetourStateMachine();

  try {
    if (sm.is(state<detoured>)) {
      auto& bot = GetBotInstance();
      if (!bot) {
        // initialize bot
        bot.reset(new PhlipBot);
        bot->Init();
      }

      // call the main bot loop
      bot->Update();

      // get the current window from the directx device
      HWND const hwnd = GetWindowFromDevice(device);

      if (!bot->is_render_initialized) {
        bot->InitRender(hwnd, device);
      }

      if (bot->is_render_initialized) {
        bot->Render(hwnd, device);
      }
    } else if (sm.is(state<shutting_down>)) {
      HADESMEM_DETAIL_TRACE_A("in shutting_down state");

      // get the current window from the directx device
      HWND const hwnd = GetWindowFromDevice(device);

      // ... shutdown stuff
      auto& bot = GetBotInstance();
      if (bot) {
        bot->Shutdown(hwnd);
        bot = nullptr;
      }

      sm.process_event(has_shutdown{});
    }
  } catch (...) {
    HADESMEM_DETAIL_TRACE_A(
      boost::current_exception_diagnostic_information().c_str());

    sm.process_event(detoured_error{});
  }

  // call the original end scene
  auto const end_scene = detour->GetTrampolineT<IDirect3DDevice9_EndScene_Fn>();
  auto ret = end_scene(device);

  return ret;
}

// Hook device reset so we can reset our internal renderer on e.g. window
// resize.
extern "C" HRESULT WINAPI IDirect3DDevice9_Reset_Detour(
  PatchDetourBase* detour, IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pp)
{
  HADESMEM_DETAIL_TRACE_FORMAT_A("Args: [%p] [%p]", device, pp);

  using namespace detour_state_machine;
  using namespace boost::sml;

  auto& sm = GetDetourStateMachine();

  try {
    if (sm.is(state<detoured>)) {
      // get the current window from the directx device
      HWND const hwnd = GetWindowFromDevice(device);

      // Reset the bot renderer if it's initialized
      auto& bot = GetBotInstance();
      if (bot && bot->is_render_initialized) {
        bot->ResetRender(hwnd);
      }
    }
  } catch (...) {
    HADESMEM_DETAIL_TRACE_A(
      boost::current_exception_diagnostic_information().c_str());

    sm.process_event(detoured_error{});
  }

  // call the original device reset
  auto const reset = detour->GetTrampolineT<IDirect3DDevice9_Reset_Fn>();
  auto ret = reset(device, pp);

  HADESMEM_DETAIL_TRACE_FORMAT_A("Ret: [%ld]", ret);

  return ret;
}

// Load is called by the bot dll loader after the bot dll has been injected
// into the WoW.exe process.
extern "C" __declspec(dllexport) unsigned int Load()
{
  HADESMEM_DETAIL_TRACE_A("phlipbot dll Load() called.");

  using namespace detour_state_machine;

  auto& sm = GetDetourStateMachine();

  sm.process_event(detoured_error{});

  sm.process_event(dll_load{});

  return EXIT_SUCCESS;
}

// Unload is called when the loader wants to shutdown the bot and eject the dll
// from the WoW.exe process.
extern "C" __declspec(dllexport) unsigned int Unload()
{
  using namespace detour_state_machine;
  using namespace boost::sml;
  using namespace std::chrono_literals;

  HADESMEM_DETAIL_TRACE_A("phlipbot dll Unload() called");

  // signal to the main thread that it's time to cleanup and shutdown
  auto& sm = GetDetourStateMachine();
  sm.process_event(dll_unload{});

  // TODO(phlip9): signal with mutex + cv
  // spin until the main thread finishes cleanup
  // (or do nothing if we're not detoured)
  while (!sm.is(state<undetouring>) && !sm.is(state<undetoured>)) {
    std::this_thread::sleep_for(10ms);
  }

  // we must undetour outside of the main thread to avoid deadlock
  if (sm.is(state<undetouring>)) {
    undetour();
    sm.process_event(has_undetoured{});
  }

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