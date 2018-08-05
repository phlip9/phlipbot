#include "window.hpp"

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

namespace phlipbot
{
HWND& GetCurrentWindow()
{
  static HWND hwnd;
  return hwnd;
}

void SetCurrentWindow(HWND hwnd) { GetCurrentWindow() = hwnd; }

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
}
