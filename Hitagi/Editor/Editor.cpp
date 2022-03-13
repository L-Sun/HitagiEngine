#include "Editor.hpp"
#include "FileIOManager.hpp"
#include "SceneManager.hpp"
#include "DebugManager.hpp"
#include "AssetManager.hpp"
#include "GuiManager.hpp"

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
    g_GuiManager->DrawGui([this]() -> void { Draw(); });
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
                m_OpenFileExt = ".fbx";
            }
            if (ImGui::MenuItem("Import MoCap")) {
                m_OpenFileExt = ".bvh";
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
    std::filesystem::path folder;
    if (m_OpenFileExt == ".fbx") {
        folder = "./Assets/Scenes";
    } else if (m_OpenFileExt == ".bvh") {
        folder = "./Assets/Motions";
    } else {
        return;
    }

    ImGui::OpenPopup("File browser");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    static std::unordered_set<std::string> file_paths;
    if (ImGui::BeginPopupModal("File browser", nullptr, ImGuiWindowFlags_Modal)) {
        for (const auto directory : std::filesystem::directory_iterator(folder)) {
            if (!directory.is_regular_file()) continue;

            auto path = directory.path();
            if (path.extension() != m_OpenFileExt) continue;
            auto path_string = path.string();

            if (ImGui::Selectable(path_string.c_str(), file_paths.count(path_string) != 0, ImGuiSelectableFlags_DontClosePopups)) {
                if (!ImGui::GetIO().KeyCtrl) {
                    file_paths.clear();
                }
                file_paths.emplace(path_string);
            }
        }

        if (ImGui::Button("Open")) {
            if (m_OpenFileExt == ".fbx") {
                for (auto&& path : file_paths) {
                    auto scene = g_AssetManager->ImportScene(path);
                    g_SceneManager->AddScene(scene);
                    g_SceneManager->SwitchScene(scene);
                }
            } else if (m_OpenFileExt == ".bvh") {
                for (auto&& path : file_paths) {
                    auto [skeleton, animation] = g_AssetManager->ImportAnimation(path);
                    g_SceneManager->GetScene()->AddSkeleton(skeleton);
                    g_SceneManager->GetScene()->AddAnimation(animation);
                }
            }
            file_paths.clear();
            m_OpenFileExt.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            file_paths.clear();
            m_OpenFileExt.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Editor::SceneExplorer() {
    if (ImGui::Begin("Scene Explorer")) {
        auto scene = g_SceneManager->GetScene();
        if (ImGui::CollapsingHeader("Scene Nodes")) {
            std::function<void(std::shared_ptr<Asset::SceneNode>)> print_node = [&](std::shared_ptr<Asset::SceneNode> node) -> void {
                if (ImGui::TreeNode(GenName(node->GetName(), node).c_str())) {
                    {
                        auto position    = node->GetPosition();
                        auto orientation = 180.0f * std::numbers::inv_pi * node->GetOrientation();
                        auto velocity    = node->GetVelocity();

                        bool changed = false;
                        changed      = ImGui::DragFloat3("Translation", position, 1.0f, 0.0f, 0.0f, "%.02f m") || changed;
                        changed      = ImGui::DragFloat3("Rotation", orientation, 1.0f, 0.0f, 0.0f, "%.03f °") || changed;
                        ImGui::DragFloat3("Velocity", velocity, 0.0f, 0.0f, 0.0f, "%.03f °");

                        if (changed) {
                            node->SetTRS(position, euler_to_quaternion(radians(orientation)), vec3f(1.0f));
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
                if (ImGui::InputText(GenName("Name", animation).c_str(), &name)) {
                    animation->SetName(name);
                }

                ImGui::SameLine();
                if (ImGui::SmallButton(GenName("Play", animation).c_str())) {
                    animation_manager.AddToPlayQueue(animation);
                    animation->Play();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(GenName("Pause", animation).c_str())) {
                    animation->Pause();
                }
                ImGui::SameLine();
                bool anima_loop = animation->IsLoop();
                if (ImGui::Checkbox(GenName("Loop", animation).c_str(), &anima_loop)) {
                    animation->SetLoop(anima_loop);
                }
            }
        }
    }

    ImGui::End();
}

std::string Editor::GenName(std::string_view name, std::shared_ptr<Asset::SceneObject> obj) {
    return fmt::format("{}##{}", name, obj->GetGuid().str());
}
std::string Editor::GenName(std::string_view name, std::shared_ptr<Asset::SceneNode> node) {
    return fmt::format("{}##{}", name, fmt::ptr(node));
}

}  // namespace Hitagi