#include "Editor.hpp"
#include "FileIOManager.hpp"
#include "SceneManager.hpp"
#include "DebugManager.hpp"
#include "AssetManager.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
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
            if (ImGui::MenuItem("Open Scene")) {
                m_OpenFileExt      = ".fbx";
                m_OpenFileExplorer = true;
            }
            if (ImGui::MenuItem("Import MoCap")) {
                m_OpenFileExt      = ".bvh";
                m_OpenFileExplorer = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (ImGui::Begin("Scenes")) {
        for (auto scene : g_SceneManager->ListAllScene()) {
            if (ImGui::Selectable(scene->GetName().c_str())) {
                g_SceneManager->SwitchScene(scene);
            }
        }
    }
    ImGui::End();
}

void Editor::FileExplorer() {
    if (m_OpenFileExplorer) {
        ImGui::OpenPopup("File browser");
    } else {
        m_OpenFileExt.clear();
    }

    if (ImGui::BeginPopupModal("File browser", &m_OpenFileExplorer, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_OpenFileExt == ".fbx") {
            for (const auto directory : std::filesystem::directory_iterator("./Assets/Scene")) {
                if (!directory.is_regular_file()) continue;

                const auto path = directory.path();
                if (path.extension() != m_OpenFileExt) continue;

                if (ImGui::Selectable(path.string().c_str())) {
                    auto scene = g_AssetManager->ImportScene(path);
                    g_SceneManager->AddScene(scene);
                    g_SceneManager->SwitchScene(scene);
                    m_OpenFileExt.clear();
                    m_OpenFileExplorer = false;
                }
            }
        } else if (m_OpenFileExt == ".bvh") {
            for (const auto directory : std::filesystem::directory_iterator("./Assets/Motion")) {
                if (!directory.is_regular_file()) continue;

                const auto path = directory.path();
                if (path.extension() != m_OpenFileExt) continue;

                if (ImGui::Selectable(path.string().c_str())) {
                    auto [skeleton, animation] = g_AssetManager->ImportAnimation(path);
                    g_SceneManager->GetScene()->AddSkeleton(skeleton);
                    g_SceneManager->GetScene()->AddAnimation(animation);

                    m_OpenFileExt.clear();
                    m_OpenFileExplorer = false;
                }
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

                        bool changed = false;
                        changed      = ImGui::DragFloat3("Translation", position, 0.01f, 0.0f, 0.0f, "%.02f m") || changed;
                        changed      = ImGui::DragFloat3("Rotation", orientation, 1.0f, 0.0f, 0.0f, "%.03f °") || changed;

                        if (changed) {
                            node->SetTRS(position, euler_to_quaternion(radians(orientation)), scaling);
                        }
                    }

                    // print children
                    for (auto&& child : node->GetChildren()) {
                        print_node(child);
                    }
                    ImGui::TreePop();
                }
            };
            print_node(scene->scene_graph);
        }

        if (ImGui::CollapsingHeader("Animation")) {
            auto& animation_manager = g_SceneManager->GetAnimationManager();
            for (auto&& animation : scene->animations) {
                auto name = animation->GetName();
                ImGui::SetNextItemWidth(100);
                if (ImGui::InputText("Name", &name)) {
                    animation->SetName(name);
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("Play##anima")) {
                    animation_manager.AddToPlayQueue(animation);
                    animation->Play();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Pause##anima")) {
                    animation->Pause();
                }
                ImGui::SameLine();
                bool anima_loop = animation->IsLoop();
                if (ImGui::Checkbox("Loop##anima", &anima_loop)) {
                    animation->SetLoop(anima_loop);
                }
            }
        }
    }

    ImGui::End();
}
}  // namespace Hitagi