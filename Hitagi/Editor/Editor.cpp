#include "Editor.hpp"
#include "FileIOManager.hpp"
#include "SceneManager.hpp"

#include <imgui.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Hitagi {
int Editor::Initialize() {
    m_Logger = spdlog::stdout_color_mt("Editor");
    m_Logger->info("Initialize...");

    return 0;
}

void Editor::Finalize() {
    m_Logger->info("Editor Finalize");
    m_Logger = nullptr;
}

void Editor::Tick() {}

void Editor::Draw() {
    // Draw
    MainMenu();
    FileExplorer();
}

void Editor::MainMenu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                m_OpenFileExplorer = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (ImGui::Begin("Scenes")) {
        for (const auto& [id, scene] : g_SceneManager->ListAllScene()) {
            if (ImGui::Selectable(scene.GetName().c_str())) {
                g_SceneManager->SwitchScene(id);
            }
        }
    }
    ImGui::End();
}

void Editor::FileExplorer() {
    if (m_OpenFileExplorer) {
        ImGui::OpenPopup("File browser");
    }
    if (ImGui::BeginPopupModal("File browser", &m_OpenFileExplorer, ImGuiWindowFlags_AlwaysAutoResize)) {
        for (const auto directory : std::filesystem::directory_iterator("./Assets/Scene")) {
            if (!directory.is_regular_file()) continue;

            const auto path = directory.path();
            if (path.extension() != ".fbx") continue;

            if (ImGui::Selectable(path.string().c_str())) {
                g_SceneManager->ImportScene(path);
                m_OpenFileExplorer = false;
            }
        }

        ImGui::EndPopup();
    }
}

}  // namespace Hitagi