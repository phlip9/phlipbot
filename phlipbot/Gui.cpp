#include "Gui.hpp"

#include <Windows.h>
#include <inttypes.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include <boost/math/constants/constants.hpp>

#include <hadesmem/detail/trace.hpp>

using boost::math::float_constants::two_pi;

namespace phlipbot
{
bool& GetGuiIsVisible()
{
  static bool is_visible{true};
  return is_visible;
}
void SetGuiIsVisible(bool val) { GetGuiIsVisible() = val; }
void ToggleGuiIsVisible() { SetGuiIsVisible(!GetGuiIsVisible()); }

Gui::Gui(ObjectManager& om, PlayerController& pc, PlayerNavigator& pn) noexcept
  : objmgr(om), player_controller(pc), player_nav(pn)
{
}

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
    auto const o_player = objmgr.GetPlayer();

    if (ImGui::CollapsingHeader("ObjectManager Testing")) {
      if (ImGui::Button("Dump Objects")) {
        for (auto obj : objmgr.IterAllObjs()) {
          HADESMEM_DETAIL_TRACE_A(obj->ToString().c_str());
        }
      }
    }

    if (ImGui::CollapsingHeader("Movement Testing")) {
      ImGui::SliderFloat("CTM dX", &ctm_dx, -30.0f, 30.0f);
      ImGui::SliderFloat("CTM dY", &ctm_dy, -30.0f, 30.0f);
      ImGui::SliderFloat("CTM Precision", &ctm_precision, 0.0f, 10.0f);

      if (ImGui::Button("Test CTM")) {
        if (o_player) {
          auto* player = o_player.value();
          auto player_pos = player->GetPosition();
          player_pos.x += ctm_dx;
          player_pos.y += ctm_dy;
          player->ClickToMove(CtmType::Move, 0, player_pos, ctm_precision);
        }
      }

      ImGui::Separator();

      {
        auto const player_pos =
          o_player.map([](auto* player) { return player->GetPosition(); })
            .value_or(vec3{0, 0, 0});

        ImGui::Text("Player Position: {%.3f, %.3f, %.3f}", player_pos.x,
                    player_pos.y, player_pos.z);
      }

      ImGui::SliderFloat("Facing Direction", &facing_direction, 0.0f, two_pi);
      ImGui::InputFloat3("Facing Position",
                         reinterpret_cast<float*>(&facing_position), 2);
      ImGui::InputScalar("Facing Object", ImGuiDataType_U64, &facing_guid,
                         nullptr, nullptr, "%016" PRIx64,
                         ImGuiInputTextFlags_CharsHexadecimal);

      if (ImGui::Button("SetFacing")) {
        if (o_player) {
          o_player.value()->SetFacing(facing_direction);
        }
      }

      ImGui::Text("Facing PID Controller");

      auto& facing_ctrl = player_controller.facing_controller;
      ImGui::SliderFloat("P Gain", &facing_ctrl.gain_p, 0.0f, 1.0f);
      ImGui::SliderFloat("I Gain", &facing_ctrl.gain_i, 0.0f, 1.0f);
      ImGui::SliderFloat("D Gain", &facing_ctrl.gain_d, 0.0f, 1.0f);

      bool on_change = false;
      on_change |= ImGui::RadioButton("Direction ", &facing_type,
                                      int(FacingType::Direction));
      ImGui::SameLine();
      on_change |= ImGui::RadioButton("Position ", &facing_type,
                                      int(FacingType::Position));
      ImGui::SameLine();
      on_change |=
        ImGui::RadioButton("Object", &facing_type, int(FacingType::Object));

      if (on_change) {
        if (facing_type == int(FacingType::Direction)) {
          player_controller.SetFacing(facing_direction);
        } else if (facing_type == int(FacingType::Position)) {
          player_controller.SetFacing(facing_position);
        } else if (facing_type == int(FacingType::Object)) {
          player_controller.SetFacing(facing_guid);
        }
      }

      if (ImGui::Button("Set Player Controller Position")) {
        player_controller.SetFacing(facing_position);
        player_controller.SetPosition(facing_position);
      }

      if (ImGui::Checkbox("Player Controller Enabled",
                          &player_controller_enabled)) {
        player_controller.SetEnabled(player_controller_enabled);
      }

      ImGui::Separator();

      ImGui::CheckboxFlags("CtmWalk", &input_flags, InputControlFlags::CtmWalk);
      ImGui::SameLine();
      ImGui::CheckboxFlags("Forward", &input_flags, InputControlFlags::Forward);
      ImGui::SameLine();
      ImGui::CheckboxFlags("Backward", &input_flags,
                           InputControlFlags::Backward);
      ImGui::SameLine();
      ImGui::CheckboxFlags("Strafe Left", &input_flags,
                           InputControlFlags::StrafeLeft);

      ImGui::CheckboxFlags("Strafe Right", &input_flags,
                           InputControlFlags::StrafeRight);
      ImGui::SameLine();
      ImGui::CheckboxFlags("Turn Left", &input_flags,
                           InputControlFlags::TurnLeft);
      ImGui::SameLine();
      ImGui::CheckboxFlags("Turn Right", &input_flags,
                           InputControlFlags::TurnRight);

      ImGui::CheckboxFlags("Auto Run", &input_flags,
                           InputControlFlags::AutoRun);
      ImGui::SameLine();
      ImGui::CheckboxFlags("All Flags", &input_flags, 0xFFFFFFFF);

      if (ImGui::Button("SetControlBits")) {
        if (o_player) {
          o_player.value()->SetControlBits(input_flags, ::GetTickCount());
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("UnsetControlBits")) {
        if (o_player) {
          o_player.value()->UnsetControlBits(input_flags, ::GetTickCount());
        }
      }
    }

    if (ImGui::CollapsingHeader("Navigation")) {
      if (ImGui::InputFloat3("Destination",
                             reinterpret_cast<float*>(&nav_destination), 2)) {
        player_nav.SetDestination(nav_destination);
      }

      if (ImGui::Checkbox("Navigation Enabled", &player_nav_enabled)) {
        player_nav.SetEnabled(player_nav_enabled);
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
