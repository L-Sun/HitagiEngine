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
        // System info
        {
            unsigned fps  = 1.0f / m_Clock.DeltaTime().count();
            auto     info = fmt::format("Memory: {} MiB | {:>3} FPS", app->GetMemoryUsage() >> 20, fps);

            ImVec2 info_size = ImGui::CalcTextSize(info.c_str());

            ImGuiStyle& style = ImGui::GetStyle();
            info_size.x += 2 * style.FramePadding.x + style.ItemSpacing.x;

            ImGui::SetCursorPos(ImVec2(ImGui::GetIO().DisplaySize.x - info_size.x, 0));
            ImGui::Text("%s", info.c_str());
        }
        ImGui::EndMainMenuBar();
    }
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

void Editor::SceneGraphViewer() {
    if (ImGui::Begin("Scene Graph Viewer")) {
        auto scene = m_SceneViewPort->GetScene();

        constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_BordersV |
                                                ImGuiTableFlags_BordersOuterH |
                                                ImGuiTableFlags_Resizable |
                                                ImGuiTableFlags_RowBg |
                                                ImGuiTableFlags_NoBordersInBody;

        constexpr ImGuiTreeNodeFlags node_flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanFullWidth;
        constexpr ImGuiTreeNodeFlags leaf_node_flags =
            ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanFullWidth;

        if (ImGui::BeginTable("Module inspector", 1, table_flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);

            std::function<void(std::shared_ptr<SceneNode>)> print_node = [&](const std::shared_ptr<SceneNode>& node) -> void {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                if (node->GetChildren().empty()) {
                    ImGui::TreeNodeEx(node->GetName().c_str(), leaf_node_flags);
                } else {
                    if (ImGui::TreeNodeEx(node->GetName().c_str(), node_flags)) {
                        // print children
                        for (auto&& child : node->GetChildren()) {
                            print_node(child);
                        }
                        ImGui::TreePop();
                    }
                }
            };

            if (scene) print_node(scene->root);

            ImGui::EndTable();
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