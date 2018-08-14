#include "Input.hpp"

#include <Shlwapi.h>

#include <imgui/imgui.h>

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

LRESULT CALLBACK ImGui_ImplDX9_WndProcHandler(HWND,
                                              UINT msg,
                                              WPARAM wParam,
                                              LPARAM lParam)
{
  ImGuiIO& io = ImGui::GetIO();
  switch (msg) {
  case WM_LBUTTONDOWN:
    io.MouseDown[0] = true;
    return true;
  case WM_LBUTTONUP:
    io.MouseDown[0] = false;
    return true;
  case WM_RBUTTONDOWN:
    io.MouseDown[1] = true;
    return true;
  case WM_RBUTTONUP:
    io.MouseDown[1] = false;
    return true;
  case WM_MBUTTONDOWN:
    io.MouseDown[2] = true;
    return true;
  case WM_MBUTTONUP:
    io.MouseDown[2] = false;
    return true;
  case WM_MOUSEWHEEL:
    io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
    return true;
  case WM_MOUSEMOVE:
    io.MousePos.x = (signed short)(lParam);
    io.MousePos.y = (signed short)(lParam >> 16);
    return true;
  case WM_KEYDOWN:
    if (wParam < 256) io.KeysDown[wParam] = 1;
    return true;
  case WM_KEYUP:
    if (wParam < 256) io.KeysDown[wParam] = 0;
    return true;
  case WM_CHAR:
    // You can also use ToAscii()+GetKeyboardState() to retrieve characters.
    if (wParam > 0 && wParam < 0x10000)
      io.AddInputCharacter((unsigned short)wParam);
    return true;
  }
  return false;
}

LRESULT CALLBACK WindowProcCallback(HWND hwnd,
                                    UINT msg,
                                    WPARAM wParam,
                                    LPARAM lParam)
{
  // TODO(phlip9): custom handler
  // TODO(phlip9): push input messages onto threadsafe message queue and
  //               process in bot update

  // Shift+F9 to toggle GUI visibility
  bool const shift_down = !!(::GetAsyncKeyState(VK_SHIFT) & 0x8000);
  if (msg == WM_KEYDOWN && !((lParam >> 30) & 0x1) && wParam == VK_F9 &&
      shift_down) {
    phlipbot::ToggleGuiIsVisible();
    return true;
  }

  // Try the ImGui input handler
  if (phlipbot::GetGuiIsVisible() &&
      ImGui_ImplDX9_WndProcHandler(hwnd, msg, wParam, lParam)) {
    return true;
  }

  // Fall through to the original handler
  auto& original_wnd_proc = GetOriginalWndProc();

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

  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->AddFontDefault();
  io.MouseDrawCursor = false;

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
}
