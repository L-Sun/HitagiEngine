#include "Editor.hpp"
#include "FileIOManager.hpp"
#include "SceneManager.hpp"
#include "DebugManager.hpp"

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

void Editor::Tick() {
    g_DebugManager->DrawAxis(scale(mat4f(1.0f), 100.0f));
}

void Editor::Draw() {
    // Draw
    MainMenu();
    FileExplorer();
    SceneExplorer();
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

void Editor::SceneExplorer() {
    if (ImGui::Begin("Scene Explorer")) {
        auto scene = g_SceneManager->GetScene();
        if (ImGui::CollapsingHeader("Scene Nodes")) {
            std::function<void(std::shared_ptr<Asset::SceneNode>)> print_node = [&](std::shared_ptr<Asset::SceneNode> node) -> void {
                if (ImGui::TreeNode(node->GetName().data())) {
                    {
                        auto position    = node->GetPosition();
                        auto orientation = 180.0f * std::numbers::inv_pi * node->GetOrientation();
                        auto scaling     = node->GetScaling();

                        bool dirty = false;
                        if (ImGui::DragFloat3("Translation", position, 0.01f, 0.0f, 0.0f, "%.02f m")) {
                            node->Translate(position - node->GetPosition());
                        }
                        if (ImGui::DragFloat3("Rotation", orientation, 1.0f, 0.0f, 0.0f, "%.03f °")) {
                            node->Rotate(radians(orientation) - node->GetOrientation());
                        }
                    }

                    // print children
                    for (auto&& child : node->GetChildren()) {
                        print_node(child);
                    }
                    ImGui::TreePop();
                }
            };
            print_node(scene.scene_graph);
        }
    }
    ImGui::End();
}
}  // namespace Hitagi