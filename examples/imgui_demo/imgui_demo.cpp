#include "imgui_demo.hpp"

#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/application.hpp>
#include <hitagi/hid/input_manager.hpp>

#include <imgui.h>

using namespace hitagi;

void ImGuiDemo::Tick() {
    bool open = true;
    gui_manager.DrawGui([&]() -> void {
        if (ImGui::Begin("Input Test")) {
            auto imgui_pos = ImGui::GetMousePos();
            ImGui::Text("%s", fmt::format("({}, {})", imgui_pos.x, imgui_pos.y).c_str());
            ImGui::Text("%s", fmt::format("Left: {}, Right: {}",
                                          ImGui::IsMouseDown(ImGuiMouseButton_Left),
                                          ImGui::IsMouseDown(ImGuiMouseButton_Right))
                                  .c_str());
            ImGui::Separator();

            ImGui::Text("%s", fmt::format("({}, {})", input_manager->GetFloat(MouseEvent::MOVE_X), input_manager->GetFloat(MouseEvent::MOVE_Y)).c_str());
            ImGui::Text("%s", fmt::format("Left: {}, Right: {}",
                                          input_manager->GetBool(VirtualKeyCode::MOUSE_L_BUTTON),
                                          input_manager->GetBool(VirtualKeyCode::MOUSE_R_BUTTON))
                                  .c_str());
        }
        ImGui::End();
        // ImGui::ShowDemoWindow(&open);
    });

    RuntimeModule::Tick();
}
