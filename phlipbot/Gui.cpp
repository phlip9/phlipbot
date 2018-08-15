#include "Gui.hpp"

#include <Windows.h>
#include <d3d9.h>
#include <inttypes.h>

#include <imgui/imgui.h>

#include <imgui/examples/directx9_example/imgui_impl_dx9.h>

#include <hadesmem/detail/trace.hpp>

#include "ObjectManager.hpp"
#include "WowUnit.hpp"
#include "wow_constants.hpp"

using namespace phlipbot::types;

namespace phlipbot
{
bool& GetGuiIsVisible()
{
  static bool is_visible{true};
  return is_visible;
}
void SetGuiIsVisible(bool val) { GetGuiIsVisible() = val; }
void ToggleGuiIsVisible() { SetGuiIsVisible(!GetGuiIsVisible()); }

void Gui::Init(HWND const hwnd, IDirect3DDevice9* device)
{
  HADESMEM_DETAIL_TRACE_FORMAT_A(
    "wnd: [%p], device: [%p], is_initialized: [%d]", hwnd, device,
    is_initialized);
  HADESMEM_DETAIL_ASSERT(!is_initialized);

  ImGui_ImplDX9_Init(hwnd, device);

  is_initialized = true;
}

void Gui::Render()
{
  HADESMEM_DETAIL_ASSERT(is_initialized);

  if (!GetGuiIsVisible()) return;

  ImGui_ImplDX9_NewFrame();

  ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiSetCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);

  if (ImGui::Begin("phlipbot")) {
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    if (ImGui::Button("Dump Objects")) {
      auto& objmgr = ObjectManager::Get();

      objmgr.EnumVisibleObjects();

      for (auto obj : objmgr.IterAllObjs()) {
        HADESMEM_DETAIL_TRACE_A(obj->ToString().c_str());
      }
    }
  }
  ImGui::End();

  ImGui::Render();
}

void Gui::Reset()
{
  HADESMEM_DETAIL_TRACE_FORMAT_A("is_initialized: [%d]", is_initialized);
  HADESMEM_DETAIL_ASSERT(is_initialized);

  ImGui_ImplDX9_InvalidateDeviceObjects();

  is_initialized = false;
}

void Gui::Shutdown()
{
  HADESMEM_DETAIL_TRACE_FORMAT_A("is_initialized: [%d]", is_initialized);
  HADESMEM_DETAIL_ASSERT(is_initialized);

  ImGui_ImplDX9_InvalidateDeviceObjects();
  ImGui_ImplDX9_Shutdown();

  is_initialized = false;
}
}
