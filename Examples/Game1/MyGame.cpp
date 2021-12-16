#include "MyGame.hpp"

#include "FileIOManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace Hitagi;

int MyGame::Initialize() {
    m_Logger = spdlog::stdout_color_mt("MyGame");
    for (const auto& scene_path : std::filesystem::directory_iterator{"./Assets/Scene"}) {
        g_SceneManager->ImportScene(scene_path);
    }
    return 0;
}

void MyGame::Finalize() {
    m_Logger->info("MyGame Finalize");
    m_Logger = nullptr;
}

void MyGame::Tick() {
    g_GuiManager->DrawGui([&]() {
        bool  my_tool_active = true;
        vec4f my_color;

        ImGui::Begin("Control Pannel", &my_tool_active, ImGuiWindowFlags_MenuBar);
        if (ImGui::BeginListBox("Scenes List")) {
            for (auto&& [id, scene] : g_SceneManager->ListAllScene()) {
                const bool is_selected = m_CurrentScene == id;

                if (ImGui::Selectable(scene.GetName().data(), is_selected))
                    m_CurrentScene = id;

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                    g_SceneManager->SwitchScene(id);
                }
            }

            ImGui::EndListBox();
        }

        ImGui::End();
    });
}
