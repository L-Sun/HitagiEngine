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
    m_ImageViewer   = static_cast<ImageViewer*>(AddSubModule(std::make_unique<ImageViewer>(engine)));
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

    if (!m_App.WindowsMinimized()) {
        auto&      renderer     = m_Engine.Renderer();
        auto&      render_graph = renderer.GetRenderGraph();
        const auto output       = render_graph.Create(
            {
                      .name        = "Editor Output",
                      .width       = m_App.GetWindowWidth(),
                      .height      = m_App.GetWindowHeight(),
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
            m_CurrScene = asset::AssetManager::Get()->ImportScene(m_FileDialog.GetSelected());
            m_SceneViewPort->SetScene(m_CurrScene);
        }
        if (ext == ".glb" || ext == ".gltf") {
            m_CurrScene = asset::AssetManager::Get()->ImportScene(m_FileDialog.GetSelected());
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

                bool node_open = ImGui::TreeNodeEx(node.get(), node_flags, "%s", node->GetName().data());
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
            const auto text      = "No Scene Node Selected!";
            const auto text_size = ImGui::CalcTextSize(text);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - text_size.x) / 2.0f);
            ImGui::SetCursorPosY((ImGui::GetWindowHeight() - text_size.y) / 2.0f);
            ImGui::Text(text);

        } else {
            if (ImGui::BeginTabBar("SceneNodeProperties")) {
                if (ImGui::BeginTabItem("Transform")) {
                    static bool local = true;
                    ImGui::Checkbox("Local", &local);

                    auto& transform = m_SelectedNode->transform;
                    if (local) {
                        ImGui::Text("Location");
                        ImGui::DragFloat3("##Location", transform.local_translation, 0.1f);

                        auto euler = math::quaternion_to_euler(transform.local_rotation);
                        ImGui::Text("Rotation");
                        ImGui::DragFloat3("##Rotation", euler, 0.1f);
                        transform.local_rotation = math::euler_to_quaternion(euler);

                        ImGui::Text("Scale");
                        ImGui::DragFloat3("##Scale", transform.local_scaling, 0.1f, 0.0f);
                    } else {
                        auto position = transform.GetPosition();
                        ImGui::Text("Position");
                        ImGui::DragFloat3("##Position", position, 0.1f);

                        auto euler = math::quaternion_to_euler(transform.GetRotation());
                        ImGui::Text("Rotation");
                        ImGui::DragFloat3("##Rotation", euler, 0.1f);

                        auto scaling = transform.GetScale();
                        ImGui::Text("Scale");
                        ImGui::DragFloat3("##Scale", scaling, 0.1f, 0.0f);

                        const auto new_world_transform    = translate(position) * rotate(euler) * scale(scaling);
                        const auto parent_world_transform = m_SelectedNode->GetParent() ? m_SelectedNode->GetParent()->transform.world_matrix : mat4f::identity();
                        const auto new_local_transform    = inverse(parent_world_transform) * new_world_transform;
                        transform                         = asset::Transform(new_local_transform);
                    }

                    ImGui::EndTabItem();
                }

                if (const auto& obj = m_SelectedNode->GetObjectRef(); obj) {
                    if (obj->GetType() == asset::Resource::Type::Mesh && ImGui::BeginTabItem("Materials")) {
                        const auto                                        mesh = std::static_pointer_cast<asset::Mesh>(obj);
                        std::pmr::unordered_set<asset::MaterialInstance*> material_instances;
                        for (const auto& sub_mesh : mesh->sub_meshes) {
                            material_instances.emplace(sub_mesh.material_instance.get());
                        }

                        for (const auto& mat_instance : material_instances) {
                            const auto material_parameter_widget = [this](asset::MaterialParameter& parameter) {
                                const auto name = parameter.name.data();
                                std::visit(
                                    utils::Overloaded{
                                        [&](float& data) { ImGui::DragFloat(name, &data); },
                                        [&](std::int32_t& data) { ImGui::DragScalar(name, ImGuiDataType_S32, &data); },
                                        [&](std::uint32_t& data) { ImGui::DragScalar(name, ImGuiDataType_U32, &data); },
                                        [&](vec2i& data) { ImGui::DragScalarN(name, ImGuiDataType_S32, data, 2); },
                                        [&](vec2u& data) { ImGui::DragScalarN(name, ImGuiDataType_U32, data, 2); },
                                        [&](vec2f& data) { ImGui::DragScalarN(name, ImGuiDataType_Float, data, 2); },
                                        [&](vec3i& data) { ImGui::DragScalarN(name, ImGuiDataType_S32, data, 3); },
                                        [&](vec3u& data) { ImGui::DragScalarN(name, ImGuiDataType_U32, data, 3); },
                                        [&](vec3f& data) { ImGui::ColorEdit3(name, data); },
                                        [&](vec4i& data) { ImGui::DragScalarN(name, ImGuiDataType_S32, data, 4); },
                                        [&](vec4u& data) { ImGui::DragScalarN(name, ImGuiDataType_U32, data, 4); },
                                        [&](vec4f& data) { ImGui::ColorEdit4(name, data); },
                                        [&](std::shared_ptr<Texture>& texture) {
                                            if (texture) {
                                                auto& rg          = m_Engine.Renderer().GetRenderGraph();
                                                auto& gui_manager = m_Engine.GuiManager();
                                                texture->InitGPUData(rg.GetDevice());
                                                ImGui::Image(gui_manager.ReadTexture(rg.Import(texture->GetGPUData())), {64, 64});
                                                if (ImGui::IsItemHovered()) {
                                                    ImGui::SetTooltip("%s", name);
                                                }
                                                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                                    m_ImageViewer->SetTexture(texture);
                                                }
                                            } else {
                                                ImGui::Text("No Texture");
                                            }
                                        },
                                        [](math::mat4f& data) {},
                                    },
                                    parameter.value);
                            };

                            auto split_parameters = mat_instance->GetSplitParameters();

                            if (ImGui::TreeNode(mat_instance, "%s", mat_instance->GetName().data())) {
                                ImGui::Text("Active Parameters");
                                for (auto& param : split_parameters.in_both) {
                                    material_parameter_widget(param);
                                    mat_instance->SetParameter(param);
                                }

                                ImGui::Text("Material Builtin Parameters");
                                ImGui::BeginDisabled(true);
                                for (auto& param : split_parameters.only_in_material) {
                                    material_parameter_widget(param);
                                }
                                ImGui::EndDisabled();

                                ImGui::Text("Unused Parameters");
                                for (auto& param : split_parameters.only_in_instance) {
                                    material_parameter_widget(param);
                                    mat_instance->SetParameter(param);
                                }

                                ImGui::TreePop();
                            }
                        }

                        ImGui::EndTabItem();
                    }

                    if (obj->GetType() == asset::Resource::Type::Camera && ImGui::BeginTabItem("Camera")) {
                        auto& parameters = std::static_pointer_cast<asset::Camera>(obj)->parameters;
                        ImGui::Text("Fov");
                        ImGui::DragFloat("##Fov", &parameters.horizontal_fov, 0.1f, 0.0f, 180.0f);
                        ImGui::Text("Near");
                        ImGui::DragFloat("##Near", &parameters.near_clip, 0.1f, 0.0f, 1000.0f);
                        ImGui::Text("Far");
                        ImGui::DragFloat("##Far", &parameters.far_clip, 0.1f, 0.0f, 1000.0f);

                        ImGui::EndTabItem();
                    }

                    if (obj->GetType() == asset::Resource::Type::Light && ImGui::BeginTabItem("Light")) {
                        auto& parameters = std::static_pointer_cast<asset::Light>(obj)->parameters;
                        ImGui::Text("Intensity");
                        ImGui::DragFloat("##Intensity", &parameters.intensity, 0.1f, 0.0f, 1000.0f);
                        ImGui::Text("Color");
                        ImGui::ColorEdit3("##Color", parameters.color);

                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
            }
        }
    }
    ImGui::End();
}

void Editor::AssetExplorer() {
    static bool open = true;
    if (ImGui::Begin("Asset Explorer", &open)) {
        for (auto mat : asset::AssetManager::Get()->GetAllMaterials()) {
        }
    }
    ImGui::End();
}

}  // namespace hitagi