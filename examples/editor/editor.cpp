#include "editor.hpp"
#include "scene_viewport.hpp"

#include <imgui.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/chrono.h>

using namespace hitagi::math;
using namespace hitagi::asset;
using namespace std::literals;

namespace hitagi {
bool Editor::Initialize() {
    RuntimeModule::Initialize();
    m_Clock.Start();

    m_SceneViewPort = static_cast<SceneViewPort*>(LoadModule(std::make_unique<SceneViewPort>()));
    m_FileDialog.SetPwd(std::filesystem::current_path() / "assets");

    return true;
}

void Editor::Tick() {
    gui_manager->DrawGui([this]() {
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        MenuBar();
        FileImporter();
        SystemInfo();
        SceneGraphViewer();
    });

    RuntimeModule::Tick();
    Render();

    m_Clock.Tick();
}

void Editor::Render() {
    auto& render_graph = graphics_manager->GetRenderGraph();

    auto back_buffer = render_graph.ImportWithoutLifeTrack("BackBuffer", &graphics_manager->GetSwapChain().GetCurrentBackBuffer());

    struct ClearPass {
        gfx::ResourceHandle back_buffer;
    };
    auto clear_pass = render_graph.AddPass<ClearPass>(
        "ClearPass",
        [&](gfx::RenderGraph::Builder& builder, ClearPass& data) {
            data.back_buffer = builder.Write(back_buffer);
        },
        [=](const gfx::RenderGraph::ResourceHelper& helper, const ClearPass& data, gfx::GraphicsCommandContext* context) {
            context->SetRenderTarget(helper.Get<gfx::Texture>(data.back_buffer));
            context->ClearRenderTarget(helper.Get<gfx::Texture>(data.back_buffer));
        });

    auto output = gui_manager->GuiRenderPass(render_graph, clear_pass.back_buffer);
    render_graph.PresentPass(output);
}

void Editor::MenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Menu")) {
            if (ImGui::BeginMenu("Import")) {
                if (ImGui::MenuItem("Motion Capture (.bvh)")) {
                    m_FileDialog.SetTitle("Import Motion Capture");
                    m_FileDialog.SetTypeFilters({".bvh"});
                    m_FileDialog.Open();
                }
                if (ImGui::MenuItem("FBX (.fbx)")) {
                    m_FileDialog.SetTitle("Import FBX");
                    m_FileDialog.SetTypeFilters({".fbx"});
                    m_FileDialog.Open();
                }
                if (ImGui::MenuItem("glTF 2.0 (.glb/.gltf)")) {
                    m_FileDialog.SetTitle("Import glTF");
                    m_FileDialog.SetTypeFilters({".glb", ".gltf"});
                    m_FileDialog.Open();
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                app->Quit();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGui::End();
}

void Editor::FileImporter() {
    m_FileDialog.Display();

    if (m_FileDialog.HasSelected()) {
        auto ext = m_FileDialog.GetSelected().extension();
        if (ext == ".bvh") {
        }
        if (ext == ".fbx") {
            m_SceneViewPort->SetScene(asset_manager->ImportScene(m_FileDialog.GetSelected()));
        }
        m_FileDialog.ClearSelected();
    }
}

void Editor::SystemInfo() {
    static bool open = true;
    if (ImGui::Begin("System Infomation", &open)) {
        // Memory usage
        {
            float    frame_time = m_Clock.DeltaTime().count() * 1000.0;
            unsigned fps        = 1000.0f / frame_time;
            ImGui::Text("%s", fmt::format("Time costing: {:>3.2} ms / {:>3} FPS", frame_time, fps).c_str());
            ImGui::Text("%s", fmt::format("Memory Usage: {} Mb", app->GetMemoryUsage() >> 20).c_str());
        }
    }
    ImGui::End();
}

void Editor::SceneGraphViewer() {
    if (ImGui::Begin("Scene Graph Viewer")) {
        auto scene = m_SceneViewPort->GetScene();
        if (scene && scene->root && ImGui::CollapsingHeader("Scene Nodes")) {
            std::function<void(std::shared_ptr<SceneNode>)> print_node = [&](const std::shared_ptr<SceneNode>& node) -> void {
                if (node == nullptr) return;
                if (ImGui::TreeNode(node->GetName().c_str())) {
                    auto& translation = node->transform.local_translation;
                    auto  orientation = rad2deg(quaternion_to_euler(node->transform.local_rotation));
                    auto& scaling     = node->transform.local_scaling;

                    ImGui::DragFloat3("Translation", translation, 1.0f, 0.0f, 0.0f, "%.02f m");

                    if (ImGui::DragFloat3("Rotation", orientation, 1.0f, 0.0f, 0.0f, "%.03f °"))
                        node->transform.local_rotation = euler_to_quaternion(deg2rad(orientation));

                    ImGui::DragFloat3("Scaling", scaling, 1.0f, 0.0f, 0.0f, "%.03f °");

                    // print children
                    for (auto&& child : node->GetChildren()) {
                        print_node(child);
                    }
                    ImGui::TreePop();
                }
            };
            print_node(scene->root);
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

void Editor::AssetExploer() {
    static bool open = true;
    if (ImGui::Begin("Asset Exploer", &open)) {
    }
    ImGui::End();
}

}  // namespace hitagi