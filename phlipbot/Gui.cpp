#include "Gui.hpp"

#include <Windows.h>
#include <d3d9.h>
#include <inttypes.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

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

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
  io.MouseDrawCursor = false;

  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX9_Init(device);

  ImGui::StyleColorsDark();

  is_initialized = true;
}

void Gui::Render()
{
  HADESMEM_DETAIL_ASSERT(is_initialized);

  if (!GetGuiIsVisible()) return;

  ImGui_ImplDX9_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

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

    ImGui::SliderFloat("CTM dX", &ctm_dx, -30.0f, 30.0f);
    ImGui::SliderFloat("CTM dY", &ctm_dy, -30.0f, 30.0f);
    ImGui::SliderFloat("CTM Precision", &ctm_precision, 0.0f, 10.0f);

    // ImGui::Checkbox("Toggle CTM", &ctm_toggle);
    // if (ctm_toggle) {
    if (ImGui::Button("Test CTM")) {
      auto& objmgr = ObjectManager::Get();
      objmgr.EnumVisibleObjects();
      auto const o_player = objmgr.GetPlayer();
      if (o_player.has_value()) {
        auto player = o_player.get();
        auto player_pos = player->GetPosition();
        player_pos.X += ctm_dx;
        player_pos.Y += ctm_dy;
        player->ClickToMove(CtmType::Move, 0, player_pos, ctm_precision);
      }
    }

    ImGui::SliderFloat("Facing", &player_facing, 0.0f, 6.282f);
    ImGui::SameLine();
    if (ImGui::Button("SetFacing")) {
      auto& objmgr = ObjectManager::Get();
      objmgr.EnumVisibleObjects();
      auto const o_player = objmgr.GetPlayer();
      if (o_player.has_value()) {
        auto player = o_player.get();
        player->SetFacing(player_facing);
      }
    }
  }
  ImGui::End();

  ImGui::EndFrame();
  ImGui::Render();
  ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
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
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  is_initialized = false;
}
} // namespace phlipbot
