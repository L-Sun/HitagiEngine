#include "editor.hpp"
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/resource/scene_manager.hpp>
#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/application.hpp>
#include <hitagi/hid/input_manager.hpp>

#include <imgui.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace hitagi::math;

namespace hitagi {
bool Editor::Initialize() {
    m_Logger = spdlog::stdout_color_mt("Editor");
    m_Logger->info("Initialize...");
    scene_manager->CurrentScene().world.RegisterSystem<DrawBone>("DrawBone");

    return true;
}

void Editor::Finalize() {
    m_Logger->info("Editor Finalize");
    m_Logger = nullptr;
}

void Editor::Tick() {
    debug_manager->DrawAxis(scale(100.0f));
    gui_manager->DrawGui([this]() -> void { Draw(); });
}

void Editor::Draw() {
    // Draw
    MainMenu();
    FileExplorer();
    // SceneExplorer();
    DebugPanel();
    debug_manager->DrawAxis(mat4f::identity());
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
        for (std::size_t index = 0; index < scene_manager->GetNumScene(); index++) {
            const auto& scene = scene_manager->GetScene(index);
            if (ImGui::Selectable(scene.name.data())) {
                scene_manager->SwitchScene(index);
            }
        }
    }
    ImGui::End();
}

void Editor::FileExplorer() {
    std::filesystem::path folder;
    if (m_OpenFileExt == ".fbx") {
        folder = "./assets/scenes";
    } else if (m_OpenFileExt == ".bvh") {
        folder = "./assets/motions";
    } else {
        return;
    }

    ImGui::OpenPopup("File browser");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("File browser", nullptr, ImGuiWindowFlags_Modal)) {
        for (const auto& directory : std::filesystem::directory_iterator(folder)) {
            if (!directory.is_regular_file()) continue;

            const auto& path = directory.path();
            if (path.extension() != m_OpenFileExt) continue;
            std::pmr::string path_string{path.string()};

            if (ImGui::Selectable(path_string.c_str(), m_SelectedFiles.contains(path_string), ImGuiSelectableFlags_DontClosePopups)) {
                if (!ImGui::GetIO().KeyCtrl) {
                    m_SelectedFiles.clear();
                }
                m_SelectedFiles.emplace(path_string);
            }
        }

        if (ImGui::Button("Open")) {
            if (m_OpenFileExt == ".fbx") {
                for (auto&& path : m_SelectedFiles) {
                    asset_manager->ImportScene(scene_manager->CurrentScene(), path);
                }
            } else if (m_OpenFileExt == ".bvh") {
                // for (auto&& path : m_SelectedFiles) {
                // auto [skeleton, animation] = asset_manager->ImportAnimation(path);
                // scene_manager->GetScene()->AddSkeleton(skeleton);
                // scene_manager->GetScene()->AddAnimation(animation);
                // }
            }
            m_SelectedFiles.clear();
            m_OpenFileExt.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_SelectedFiles.clear();
            m_OpenFileExt.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Editor::SceneExplorer() {
    if (ImGui::Begin("Scene Explorer")) {
        auto& scene = scene_manager->CurrentScene();
        if (ImGui::CollapsingHeader("Scene Nodes")) {
            std::function<void(ecs::Entity)> print_node = [&](ecs::Entity node) -> void {
                auto meta      = scene.world.AccessEntity<resource::MetaInfo>(node);
                auto transform = scene.world.AccessEntity<resource::Transform>(node);

                std::pmr::vector<ecs::Entity> children;
                for (const auto& arche : scene.world.GetArchetypes<resource::Hierarchy>()) {
                    auto entities = arche->GetComponentArray<ecs::Entity>();
                    auto hiers    = arche->GetComponentArray<resource::Hierarchy>();
                    for (std::size_t i = 0; i < entities.size(); i++) {
                        if (hiers[i].parentID == node) {
                            children.emplace_back(entities[i]);
                        }
                    }
                }
                std::sort(children.begin(), children.end());

                if (ImGui::TreeNode(meta.has_value() ? meta->get().name.c_str() : "Unkown")) {
                    if (transform.has_value()) {
                        auto& translation = transform->get().local_translation;
                        auto  orientation = rad2deg(quaternion_to_euler(transform->get().local_rotation));
                        auto& scaling     = transform->get().local_scaling;

                        ImGui::DragFloat3("Translation", translation, 1.0f, 0.0f, 0.0f, "%.02f m");

                        if (ImGui::DragFloat3("Rotation", orientation, 1.0f, 0.0f, 0.0f, "%.03f °"))
                            transform->get().local_rotation = euler_to_quaternion(deg2rad(orientation));

                        ImGui::DragFloat3("Scaling", scaling, 1.0f, 0.0f, 0.0f, "%.03f °");
                    }

                    // print children
                    for (auto&& child : children) {
                        print_node(child);
                    }
                    ImGui::TreePop();
                }
            };
            print_node(scene.root);
        }

        if (ImGui::CollapsingHeader("Animation")) {
            // auto& animation_manager = scene_manager->GetAnimationManager();
            // for (auto&& animation : scene->animations) {
            //     auto name = animation->GetName();
            //     ImGui::SetNextItemWidth(100);
            //     if (ImGui::InputText(GenName("Name", animation).c_str(), &name)) {
            //         animation->SetName(name);
            //     }

            //     ImGui::SameLine();
            //     if (ImGui::SmallButton(GenName("Play", animation).c_str())) {
            //         animation_manager.AddToPlayQueue(animation);
            //         animation->Play();
            //     }
            //     ImGui::SameLine();
            //     if (ImGui::SmallButton(GenName("Pause", animation).c_str())) {
            //         animation->Pause();
            //     }
            //     ImGui::SameLine();
            //     bool anima_loop = animation->IsLoop();
            //     if (ImGui::Checkbox(GenName("Loop", animation).c_str(), &anima_loop)) {
            //         animation->SetLoop(anima_loop);
            //     }
            // }
        }
    }

    ImGui::End();
}

void Editor::DebugPanel() {
    if (input_manager->GetBoolNew(VirtualKeyCode::KEY_D)) {
        debug_manager->ToggleDebugInfo();
    }

    gui_manager->DrawGui([]() {
        if (ImGui::Begin("Debug Pannel")) {
            ImGui::Text("%s", fmt::format("Num debug primitives: {}", debug_manager->GetNumPrimitives()).c_str());
            ImGui::Text("%s", fmt::format("Memory Usage: {}Mb", app->GetMemoryUsage() >> 20).c_str());
            ImGui::Checkbox("DrawBone", &DrawBone::enable);
            ImGui::Checkbox("Transform", &resource::TransformSystem::enable);
        }
        ImGui::End();
        bool open = true;
        ImGui::ShowMetricsWindow(&open);
    });
}

bool Editor::DrawBone::enable = false;

void Editor::DrawBone::OnUpdate(ecs::Schedule& schedule, std::chrono::duration<double> delta) {
    if (!enable) return;
    schedule.Request("DrawBone", [&](ecs::Entity id, const resource::Armature& armature) {
        for (auto bone : armature.bone_collection) {
            auto parent = schedule.world.AccessEntity<resource::Hierarchy>(bone)->get().parentID;
            if (parent != id) {
                auto from = schedule.world.AccessEntity<resource::Transform>(bone)->get().GetPosition();
                auto to   = schedule.world.AccessEntity<resource::Transform>(parent)->get().GetPosition();
                debug_manager->DrawLine(from, to, vec4f{0.2, 0.3, 0.7, 1.0});
            }
        }
    });
}

}  // namespace hitagi