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
auto get_resource_label(Resource* res) {
    return fmt::format("{}##{}", res->GetName(), res->GetGuid().str());
}

Editor::Editor(Engine& engine)
    : RuntimeModule("Editor"), m_Engine(engine), m_App(engine.App()) {
    m_Clock.Start();

    m_SceneViewPort = static_cast<SceneViewPort*>(AddSubModule(std::make_unique<SceneViewPort>(engine)));
    m_FileDialog.SetPwd(m_App.GetConfig().asset_root_path);
}

void Editor::Tick() {
    if (m_CurrScene) m_CurrScene->Update();

    m_Engine.GuiManager().DrawGui([this]() {
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        MenuBar();
        FileImporter();
        SceneGraphViewer();
        SceneNodeModifier();
    });

    RuntimeModule::Tick();

    {
        auto& renderer     = m_Engine.Renderer();
        auto& render_graph = renderer.GetRenderGraph();
        auto  output       = render_graph.Create(
            {
                       .name        = "Editor Output",
                       .width       = renderer.GetSwapChain().GetWidth(),
                       .height      = renderer.GetSwapChain().GetHeight(),
                       .format      = gfx::Format::R8G8B8A8_UNORM,
                       .clear_value = math::vec4f{0.0f, 0.0f, 0.0f, 1.0f},
                       .usages      = gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::RenderTarget,
            },
            "Editor Output");
        renderer.RenderGui(output, true);
        renderer.ToSwapChain(output);
    }

    m_Clock.Tick();
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
                m_App.Quit();
            }
            ImGui::EndMenu();
        }
        // System info
        {
            static std::uint64_t smooth_count = 0;
            static float         frame_time   = 0;
            if (smooth_count < m_Clock.TotalTime().count()) {
                smooth_count++;
                frame_time = m_Engine.Renderer().GetFrameTime().count();
            }

            auto info = fmt::format("Memory: {:>4} MiB | {:>4} FPS", m_App.GetMemoryUsage() >> 20, static_cast<unsigned>(1.0f / frame_time));

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
            m_CurrScene = asset_manager->ImportScene(m_FileDialog.GetSelected());
            m_SceneViewPort->SetScene(m_CurrScene);
        }
        m_FileDialog.ClearSelected();
    }
}

void Editor::SceneGraphViewer() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    if (ImGui::Begin("Scene Graph Viewer")) {
        auto scene = m_SceneViewPort->GetScene();

        constexpr ImGuiTableFlags table_flags =
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg;
        constexpr ImGuiTreeNodeFlags base_node_flags =
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanFullWidth;

        if (ImGui::BeginTable("Scene Graph", 1, table_flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
            int num_row = 0;

            std::function<void(std::shared_ptr<SceneNode>)> print_node = [&](const std::shared_ptr<SceneNode>& node) -> void {
                num_row++;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                auto node_flags = base_node_flags;
                if (m_SelectedNode == node) {
                    node_flags |= ImGuiTreeNodeFlags_Selected;
                }
                node_flags |= node->GetChildren().empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow;

                bool node_open = ImGui::TreeNodeEx(node.get(), node_flags, "%s", node->GetName().c_str());
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
                    m_SelectedNode = node;

                if (node_open && !(node_flags & ImGuiTreeNodeFlags_Leaf)) {
                    // print children
                    for (auto&& child : node->GetChildren()) {
                        print_node(child);
                    }
                    ImGui::TreePop();
                }
            };

            if (scene) print_node(scene->root);

            // Add empty row
            for (int i = 0; i < std::max(0, 10 - num_row); i++) {
                ImGui::TableNextRow(0, ImGui::GetTextLineHeight());
            }

            ImGui::EndTable();
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_SelectedNode = nullptr;
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void Editor::SceneNodeModifier() {
    if (ImGui::Begin("Scene Node Modifier")) {
        if (m_SelectedNode == nullptr) {
            ImGui::Text("No Scene Node Selected!");
        } else if (ImGui::BeginTable("Space Properties", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Property Name", ImGuiTableColumnFlags_NoHide);

            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Translation");
                ImGui::TableNextColumn();
                ImGui::DragFloat3("##Translation", m_SelectedNode->transform.local_translation);
            }
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Rotation");
                ImGui::TableNextColumn();
                ImGui::DragFloat3("##Rotation", m_SelectedNode->transform.local_rotation);
            }
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Scaling");
                ImGui::TableNextColumn();
                ImGui::DragFloat3("##Scaling", m_SelectedNode->transform.local_scaling);
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void Editor::AssetExplorer() {
    static bool open = true;
    if (ImGui::Begin("Asset Explorer", &open)) {
        for (auto mat : asset_manager->GetAllMaterials()) {
        }
    }
    ImGui::End();
}

}  // namespace hitagi