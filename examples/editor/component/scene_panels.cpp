module;

#include <imgui.h>
#include <imgui_internal.h>
#include <tracy/Tracy.hpp>

module editor;

using namespace hitagi::math;
using namespace hitagi::asset;
using namespace std::literals;

namespace hitagi {
namespace {
constexpr auto kWorkbenchWindowFlags = ImGuiWindowFlags_NoCollapse;

}  // namespace

auto IsDescendantOf(ecs::Entity entity, ecs::Entity possible_parent) -> bool {
    if (!entity || !possible_parent || !possible_parent.Has<asset::RelationShip>()) return false;
    for (const auto child : possible_parent.Get<asset::RelationShip>().GetChildren()) {
        if (child == entity || IsDescendantOf(entity, child)) return true;
    }
    return false;
}
void Editor::SceneGraphViewer() {
    if (!m_State.IsPanelVisible(EditorPanel::SceneGraph)) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    auto open = true;
    if (ImGui::Begin("Hierarchy", &open, kWorkbenchWindowFlags)) {
        auto scene = m_SceneViewPort->GetScene();

        constexpr ImGuiTableFlags table_flags =
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg;
        constexpr ImGuiTreeNodeFlags base_node_flags =
            ImGuiTreeNodeFlags_SpanFullWidth;

        if (ImGui::BeginTable("Scene Graph", 1, table_flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
            int num_row = 0;

            std::function<void(ecs::Entity)> print_node = [&](const ecs::Entity entity) -> void {
                num_row++;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                auto node_flags = base_node_flags;
                if (m_State.GetSelectedEntity() == entity) {
                    node_flags |= ImGuiTreeNodeFlags_Selected;
                }

                if (entity.Has<asset::RelationShip>()) {
                    if (entity.Get<asset::RelationShip>().GetChildren().empty()) {
                        node_flags |= ImGuiTreeNodeFlags_Leaf;
                    } else {
                        node_flags |= ImGuiTreeNodeFlags_OpenOnArrow;
                    }
                }

                std::pmr::string name;
                std::pmr::string name_id;
                if (entity.Has<asset::MetaInfo>()) {
                    name    = entity.Get<asset::MetaInfo>().name;
                    name_id = std::format("{}-{}", name, entity);
                } else {
                    name    = std::format("{}", entity);
                    name_id = name;
                }

                if (ImGui::TreeNodeEx(name_id.c_str(), node_flags, "%s", name.c_str())) {
                    // print children
                    if (!(node_flags & ImGuiTreeNodeFlags_Leaf)) {
                        for (const auto child : entity.Get<asset::RelationShip>().GetChildren()) {
                            print_node(child);
                        }
                    }
                    ImGui::TreePop();
                }

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
                    m_State.SelectEntity(entity);
                }

                if (ImGui::BeginDragDropSource()) {
                    auto payload_entity = entity;
                    ImGui::SetDragDropPayload("HITAGI_ENTITY", &payload_entity, sizeof(payload_entity));
                    ImGui::Text("%s", name.c_str());
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const auto* payload = ImGui::AcceptDragDropPayload("HITAGI_ENTITY")) {
                        auto dropped = *static_cast<const ecs::Entity*>(payload->Data);
                        if (scene && dropped && dropped != entity && !IsDescendantOf(entity, dropped)) {
                            const auto old_parent = dropped.Has<asset::RelationShip>() ? dropped.Get<asset::RelationShip>().parent : ecs::Entity{};
                            ExecuteCommand(std::make_unique<ReparentEntityCommand>(*scene, dropped, old_parent, entity));
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Create Child")) {
                        ExecuteCommand(std::make_unique<CreateEmptyEntityCommand>(*scene, entity, "Empty Entity", &m_State));
                    }
                    if (entity != scene->GetRootEntity() && ImGui::MenuItem("Duplicate")) {
                        ExecuteCommand(std::make_unique<DuplicateEntitySubtreeCommand>(*scene, entity, &m_State));
                    }
                    if (entity != scene->GetRootEntity() && ImGui::MenuItem("Delete")) {
                        ExecuteCommand(std::make_unique<DeleteEntitySubtreeCommand>(*scene, entity, &m_State));
                    }
                    ImGui::EndPopup();
                }
            };

            if (scene) print_node(scene->GetRootEntity());

            // // Add empty row
            // for (int i = 0; i < std::max(0, 10 - num_row); i++) {
            //     ImGui::TableNextRow(0, ImGui::GetTextLineHeight());
            // }

            ImGui::EndTable();
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_State.ClearSelection();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    if (!open) m_State.SetPanelVisible(EditorPanel::SceneGraph, false);
}

void Editor::SceneNodeModifier() {
    if (!m_State.IsPanelVisible(EditorPanel::SceneNodeModifier)) return;

    auto open = true;
    if (ImGui::Begin("Inspector", &open, kWorkbenchWindowFlags)) {
        auto selected_entity = m_State.GetSelectedEntity();
        if (!selected_entity) {
            const auto text      = "No Scene Node Selected!";
            const auto text_size = ImGui::CalcTextSize(text);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - text_size.x) / 2.0f);
            ImGui::SetCursorPosY((ImGui::GetWindowHeight() - text_size.y) / 2.0f);
            ImGui::Text(text);

        } else {
            if (selected_entity.Has<asset::MetaInfo>()) {
                static std::array<char, 256> name_buffer{};
                static ecs::Entity           buffered_entity;

                if (buffered_entity != selected_entity) {
                    buffered_entity = selected_entity;
                    name_buffer.fill('\0');
                    const auto& name = selected_entity.Get<asset::MetaInfo>().name;
                    std::ranges::copy(name.substr(0, name_buffer.size() - 1), name_buffer.begin());
                }

                ImGui::Text("Name");
                if (ImGui::InputText("##EntityName", name_buffer.data(), name_buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue) ||
                    ImGui::IsItemDeactivatedAfterEdit()) {
                    const auto new_name = std::string_view{name_buffer.data()};
                    const auto old_name = std::string_view{selected_entity.Get<asset::MetaInfo>().name};
                    if (!new_name.empty() && new_name != old_name) {
                        ExecuteCommand(std::make_unique<RenameEntityCommand>(*m_State.GetCurrentScene(), selected_entity, old_name, new_name));
                    }
                }
                ImGui::Separator();
            }

            if (auto scene = m_State.GetCurrentScene()) {
                if (ImGui::Button("Add Component")) {
                    ImGui::OpenPopup("AddComponentPopup");
                }
                if (ImGui::BeginPopup("AddComponentPopup")) {
                    if (selected_entity.Has<CameraComponent>()) {
                        ImGui::BeginDisabled(true);
                        ImGui::MenuItem("Camera");
                        ImGui::EndDisabled();
                    } else if (ImGui::MenuItem("Camera")) {
                        ExecuteCommand(std::make_unique<AddSceneComponentCommand>(
                            *scene,
                            selected_entity,
                            SceneComponentKind::Camera,
                            "Camera"));
                    }

                    if (selected_entity.Has<LightComponent>()) {
                        ImGui::BeginDisabled(true);
                        ImGui::MenuItem("Light");
                        ImGui::EndDisabled();
                    } else if (ImGui::MenuItem("Light")) {
                        ExecuteCommand(std::make_unique<AddSceneComponentCommand>(
                            *scene,
                            selected_entity,
                            SceneComponentKind::Light,
                            "Light"));
                    }
                    ImGui::EndPopup();
                }
                ImGui::Separator();
            }

            if (ImGui::BeginTabBar("SceneNodeProperties")) {
                if (selected_entity.Has<asset::Transform>()) {
                    auto& transform = selected_entity.Get<asset::Transform>();
                    if (ImGui::BeginTabItem("Transform")) {
                        static bool                            local = true;
                        static std::optional<asset::Transform> transform_edit_start;
                        const auto                             commit_transform_edit = [&]() {
                            if (ImGui::IsItemDeactivatedAfterEdit() && transform_edit_start) {
                                ExecuteCommand(std::make_unique<TransformChangeCommand>(selected_entity, *transform_edit_start, transform));
                                transform_edit_start.reset();
                            }
                        };
                        ImGui::Checkbox("Local", &local);

                        if (local) {
                            ImGui::Text("Location");
                            auto before = transform;
                            ImGui::DragFloat3("##Location", transform.position, 0.1f);
                            if (ImGui::IsItemActivated()) transform_edit_start = before;
                            commit_transform_edit();

                            auto euler = math::quaternion_to_euler(transform.rotation);
                            ImGui::Text("Rotation");
                            before = transform;
                            ImGui::DragFloat3("##Rotation", euler, 0.1f);
                            transform.rotation = math::euler_to_quaternion(euler);
                            if (ImGui::IsItemActivated()) transform_edit_start = before;
                            commit_transform_edit();

                            ImGui::Text("Scale");
                            before = transform;
                            ImGui::DragFloat3("##Scale", transform.scaling, 0.1f, 0.0f);
                            if (ImGui::IsItemActivated()) transform_edit_start = before;
                            commit_transform_edit();
                        } else {
                            auto [global_position, global_rotation, global_scaling] = decompose(transform.world_matrix);

                            ImGui::Text("Position");
                            auto before = transform;
                            ImGui::DragFloat3("##Position", global_position, 0.1f);
                            if (ImGui::IsItemActivated()) transform_edit_start = before;

                            auto euler = math::quaternion_to_euler(global_rotation);
                            ImGui::Text("Rotation");
                            before = transform_edit_start.value_or(transform);
                            ImGui::DragFloat3("##Rotation", euler, 0.1f);
                            if (ImGui::IsItemActivated()) transform_edit_start = before;

                            ImGui::Text("Scale");
                            before = transform_edit_start.value_or(transform);
                            ImGui::DragFloat3("##Scale", global_scaling, 0.1f, 0.0f);
                            if (ImGui::IsItemActivated()) transform_edit_start = before;

                            const auto new_world_transform    = translate(global_position) * rotate(euler) * scale(global_scaling);
                            mat4f      parent_world_transform = transform.world_matrix * inverse(translate(global_position) * rotate(euler) * scale(global_scaling));

                            const auto new_local_transform                       = inverse(parent_world_transform) * new_world_transform;
                            auto [local_position, local_rotation, local_scaling] = decompose(new_local_transform);
                            transform.position                                   = local_position;
                            transform.rotation                                   = local_rotation;
                            transform.scaling                                    = local_scaling;
                            commit_transform_edit();
                        }

                        ImGui::EndTabItem();
                    }
                }

                if (selected_entity.Has<asset::MeshComponent>() && ImGui::BeginTabItem("Materials")) {
                    const auto mesh = selected_entity.Get<asset::MeshComponent>().mesh;

                    std::pmr::vector<std::shared_ptr<asset::MaterialInstance>> material_instances;
                    for (const auto& sub_mesh : mesh->sub_meshes) {
                        if (!sub_mesh.material_instance) continue;
                        const auto duplicated = std::ranges::any_of(material_instances, [&](const auto& material_instance) {
                            return material_instance.get() == sub_mesh.material_instance.get();
                        });
                        if (!duplicated) material_instances.emplace_back(sub_mesh.material_instance);
                    }

                    for (const auto& mat_instance : material_instances) {
                        const auto material_parameter_widget = [this, &mat_instance](asset::MaterialParameter& parameter) {
                            const auto name   = parameter.name.data();
                            const auto edited = std::visit(
                                utils::Overloaded{
                                    [&](float& data) { return ImGui::DragFloat(name, &data); },
                                    [&](std::int32_t& data) { return ImGui::DragScalar(name, ImGuiDataType_S32, &data); },
                                    [&](std::uint32_t& data) { return ImGui::DragScalar(name, ImGuiDataType_U32, &data); },
                                    [&](vec2i& data) { return ImGui::DragScalarN(name, ImGuiDataType_S32, data, 2); },
                                    [&](vec2u& data) { return ImGui::DragScalarN(name, ImGuiDataType_U32, data, 2); },
                                    [&](vec2f& data) { return ImGui::DragScalarN(name, ImGuiDataType_Float, data, 2); },
                                    [&](vec3i& data) { return ImGui::DragScalarN(name, ImGuiDataType_S32, data, 3); },
                                    [&](vec3u& data) { return ImGui::DragScalarN(name, ImGuiDataType_U32, data, 3); },
                                    [&](vec3f& data) { return ImGui::DragScalarN(name, ImGuiDataType_Float, data, 3); },
                                    [&](vec4i& data) { return ImGui::DragScalarN(name, ImGuiDataType_S32, data, 4); },
                                    [&](vec4u& data) { return ImGui::DragScalarN(name, ImGuiDataType_U32, data, 4); },
                                    [&](vec4f& data) { return ImGui::DragScalarN(name, ImGuiDataType_Float, data, 4); },
                                    [&](Color& data) { return ImGui::ColorEdit4(name, data); },
                                    [&](std::shared_ptr<Texture>& texture) {
                                        if (texture) {
                                            auto& rg          = m_Engine.RenderRuntime().GetRenderGraph();
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
                                        return false;
                                    },
                                    [](math::mat4f& _) { return false; },
                                },
                                parameter.value);
                            static std::optional<asset::MaterialParameter> material_edit_start;
                            static asset::MaterialInstance*                material_edit_instance = nullptr;
                            if (ImGui::IsItemActivated()) {
                                material_edit_start    = parameter;
                                material_edit_instance = mat_instance.get();
                            }
                            if (edited) {
                                mat_instance->SetParameter(parameter);
                            }
                            if (ImGui::IsItemDeactivatedAfterEdit() && material_edit_start && material_edit_instance == mat_instance.get()) {
                                ExecuteCommand(std::make_unique<MaterialParameterChangeCommand>(
                                    mat_instance,
                                    *material_edit_start,
                                    parameter));
                                material_edit_start.reset();
                                material_edit_instance = nullptr;
                            }
                        };

                        auto split_parameters = mat_instance->GetSplitParameters();

                        if (ImGui::TreeNode(mat_instance.get(), "%s", mat_instance->GetName().data())) {
                            ImGui::Text("Active Parameters");
                            for (auto& param : split_parameters.in_both) {
                                material_parameter_widget(param);
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
                            }

                            ImGui::TreePop();
                        }
                    }

                    ImGui::EndTabItem();
                }

                if (selected_entity.Has<CameraComponent>() && ImGui::BeginTabItem("Camera")) {
                    if (ImGui::Button("Remove Camera")) {
                        ExecuteCommand(std::make_unique<RemoveSceneComponentCommand>(
                            *m_State.GetCurrentScene(),
                            selected_entity,
                            SceneComponentKind::Camera));
                        ImGui::EndTabItem();
                    } else {
                        auto&                                    parameters = selected_entity.Get<CameraComponent>().camera->parameters;
                        static std::optional<Camera::Parameters> camera_edit_start;
                        const auto                               commit_camera_edit = [&]() {
                            if (ImGui::IsItemDeactivatedAfterEdit() && camera_edit_start) {
                                ExecuteCommand(std::make_unique<CameraParameterChangeCommand>(
                                    selected_entity.Get<CameraComponent>().camera,
                                    *camera_edit_start,
                                    parameters));
                                camera_edit_start.reset();
                            }
                        };

                        ImGui::Text("Fov");
                        auto before = parameters;
                        ImGui::DragFloat("##Fov", &parameters.horizontal_fov, 0.1f, 0.0f, 180.0f);
                        if (ImGui::IsItemActivated()) camera_edit_start = before;
                        commit_camera_edit();
                        ImGui::Text("Near");
                        before = parameters;
                        ImGui::DragFloat("##Near", &parameters.near_clip, 0.1f, 0.0f, 1000.0f);
                        if (ImGui::IsItemActivated()) camera_edit_start = before;
                        commit_camera_edit();
                        ImGui::Text("Far");
                        before = parameters;
                        ImGui::DragFloat("##Far", &parameters.far_clip, 0.1f, 0.0f, 1000.0f);
                        if (ImGui::IsItemActivated()) camera_edit_start = before;
                        commit_camera_edit();

                        ImGui::EndTabItem();
                    }
                }

                if (selected_entity.Has<LightComponent>() && ImGui::BeginTabItem("Light")) {
                    if (ImGui::Button("Remove Light")) {
                        ExecuteCommand(std::make_unique<RemoveSceneComponentCommand>(
                            *m_State.GetCurrentScene(),
                            selected_entity,
                            SceneComponentKind::Light));
                        ImGui::EndTabItem();
                    } else {
                        auto&                                   parameters = selected_entity.Get<LightComponent>().light->parameters;
                        static std::optional<Light::Parameters> light_edit_start;
                        const auto                              commit_light_edit = [&]() {
                            if (ImGui::IsItemDeactivatedAfterEdit() && light_edit_start) {
                                ExecuteCommand(std::make_unique<LightParameterChangeCommand>(
                                    selected_entity.Get<LightComponent>().light,
                                    *light_edit_start,
                                    parameters));
                                light_edit_start.reset();
                            }
                        };

                        ImGui::Text("Intensity");
                        auto before = parameters;
                        ImGui::DragFloat("##Intensity", &parameters.intensity, 0.1f, 0.0f, 1000.0f);
                        if (ImGui::IsItemActivated()) light_edit_start = before;
                        commit_light_edit();
                        ImGui::Text("Color");
                        before = parameters;
                        ImGui::ColorEdit3("##Color", parameters.color);
                        if (ImGui::IsItemActivated()) light_edit_start = before;
                        commit_light_edit();

                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
            }
        }
    }
    ImGui::End();
    if (!open) m_State.SetPanelVisible(EditorPanel::SceneNodeModifier, false);
}
}  // namespace hitagi
