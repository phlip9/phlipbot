#include "Input.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>

#include <hadesmem/detail/assert.hpp>
#include <hadesmem/detail/trace.hpp>

#include "Gui.hpp"

// Eww... but can't figure out how to work around WndProc callbacks executing
// in different scope...
WNDPROC& GetOriginalWndProc()
{
  static WNDPROC original_wnd_proc{nullptr};
  return original_wnd_proc;
}
void SetOriginalWndProc(WNDPROC const wnd_proc)
{
  GetOriginalWndProc() = wnd_proc;
}

// Definition in imgui/examples/imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                              UINT msg,
                                              WPARAM wParam,
                                              LPARAM lParam);

LRESULT CALLBACK WindowProcCallback(HWND hwnd,
                                    UINT msg,
                                    WPARAM wParam,
                                    LPARAM lParam)
{
  // TODO(phlip9): custom handler
  // TODO(phlip9): push input messages onto threadsafe message queue and
  //               process in bot update

  auto& original_wnd_proc = GetOriginalWndProc();

  // TODO(phlip9): signal shutdown to detour state machine
  //if (msg == WM_CLOSE && original_wnd_proc &&
  //    CallWindowProc(original_wnd_proc, hwnd, msg, wParam, lParam)) {
  //  return true;
  //}

  // Shift+F9 to toggle GUI visibility
  bool const shift_down = !!(::GetAsyncKeyState(VK_SHIFT) & 0x8000);
  if (msg == WM_KEYDOWN && !((lParam >> 30) & 0x1) && wParam == VK_F9 &&
      shift_down) {
    phlipbot::ToggleGuiIsVisible();
    return true;
  }

  // TODO(phlip9): only capture input when focused on the window?
  // Use the ImGui input handler when the Gui is visible
  if (phlipbot::GetGuiIsVisible()) {
    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
    return true;
  }

  // Fall through to the original handler
  if (original_wnd_proc &&
      CallWindowProc(original_wnd_proc, hwnd, msg, wParam, lParam)) {
    return true;
  }

  return false;
}

namespace phlipbot
{
void Input::HookInput(HWND const hwnd)
{
  HADESMEM_DETAIL_TRACE_FORMAT_A("hwd: [%p], is_hooked: [%d]", hwnd, is_hooked);
  HADESMEM_DETAIL_ASSERT(!is_hooked);

  // Hook the original wnd proc
  if (!GetOriginalWndProc()) {
    auto const wnd_proc = reinterpret_cast<LONG>(WindowProcCallback);
    SetOriginalWndProc(
      reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWL_WNDPROC, wnd_proc)));
  }

  is_hooked = true;
}

void Input::UnhookInput(HWND const hwnd)
{
  HADESMEM_DETAIL_TRACE_FORMAT_A("hwd: [%p], is_hooked: [%d]", hwnd, is_hooked);
  HADESMEM_DETAIL_ASSERT(is_hooked);

  // unhook our window handler and replace it with the original
  SetWindowLongPtr(hwnd, GWL_WNDPROC,
                   reinterpret_cast<LONG>(GetOriginalWndProc()));

  SetOriginalWndProc(nullptr);

  is_hooked = false;
}
} // namespace phlipbot
