module;

#include "interop/tracy_macros.hpp"

module editor;
import interop.imgui;
import interop.tracy;

using namespace hitagi::math;
using namespace hitagi::asset;
using namespace std::literals;

namespace hitagi {
namespace {
constexpr auto kWorkbenchWindowFlags = ImGuiWindowFlags_NoCollapse;

auto MaterialSourceTypeName(MaterialSourceType type) noexcept -> std::string_view {
    switch (type) {
        case MaterialSourceType::Builtin:
            return "Builtin";
        case MaterialSourceType::Imported:
            return "Imported";
        case MaterialSourceType::MDL:
            return "MDL";
        case MaterialSourceType::JSON:
            return "JSON";
        case MaterialSourceType::Generated:
            return "Generated";
        case MaterialSourceType::Unknown:
            return "Unknown";
    }
    return "Unknown";
}

auto MaterialParameterValueTypeName(const asset::MaterialParameterValue& value) noexcept -> std::string_view {
    return std::visit(
        utils::Overloaded{
            [](const float&) -> std::string_view { return "float"; },
            [](const std::int32_t&) -> std::string_view { return "int"; },
            [](const std::uint32_t&) -> std::string_view { return "uint"; },
            [](const math::vec2i&) -> std::string_view { return "int2"; },
            [](const math::vec2u&) -> std::string_view { return "uint2"; },
            [](const math::vec2f&) -> std::string_view { return "float2"; },
            [](const math::vec3i&) -> std::string_view { return "int3"; },
            [](const math::vec3u&) -> std::string_view { return "uint3"; },
            [](const math::vec3f&) -> std::string_view { return "float3"; },
            [](const math::vec4i&) -> std::string_view { return "int4"; },
            [](const math::vec4u&) -> std::string_view { return "uint4"; },
            [](const math::vec4f&) -> std::string_view { return "float4"; },
            [](const math::Color&) -> std::string_view { return "color"; },
            [](const math::mat4f&) -> std::string_view { return "float4x4"; },
            [](const std::shared_ptr<asset::Texture>&) -> std::string_view { return "texture"; },
        },
        value);
}

auto MaterialPassParameterTypeName(const asset::Material& material, std::string_view name) noexcept -> std::string_view {
    const auto iter = std::ranges::find_if(material.GetParameters(), [name](const auto& parameter) {
        return parameter.name == name;
    });
    return iter == material.GetParameters().end() ? "unknown" : MaterialParameterValueTypeName(iter->value);
}

auto ResolvePassTextures(const asset::Material& material, const asset::MaterialPass& pass) -> std::pmr::vector<std::shared_ptr<asset::Texture>> {
    std::pmr::vector<std::shared_ptr<asset::Texture>> textures;
    textures.resize(pass.bindings.size());
    for (std::size_t binding_index = 0; binding_index < pass.bindings.size(); ++binding_index) {
        const auto& binding   = pass.bindings[binding_index];
        const auto  parameter = material.GetParameter<std::shared_ptr<asset::Texture>>(binding);
        if (parameter) textures[binding_index] = *parameter;
    }
    return textures;
}

auto HasMaterialSourceInfo(const MaterialSourceInfo& source_info) noexcept -> bool {
    return source_info.type != MaterialSourceType::Unknown ||
           !source_info.source_asset.empty() ||
           !source_info.source_material_path.empty() ||
           !source_info.source_shader_id.empty() ||
           !source_info.source_texture_paths.empty() ||
           !source_info.unsupported_inputs.empty() ||
           !source_info.unsupported_nodes.empty();
}

void DrawTextureSlotPreview(Engine& engine, std::string_view label, const std::shared_ptr<asset::Texture>& texture, float size = 48.0f) {
    if (!texture) {
        ImGui::TextUnformatted("-");
        return;
    }

    auto& render_graph = engine.RenderRuntime().GetRenderGraph();
    texture->Load(engine.ResourceLoadContext());
    ImGui::Image(engine.GuiManager().ReadTexture(render_graph.Import(texture->GetGPUData())), ImVec2(size, size));
    if (ImGui::IsItemHovered()) {
        const auto& path = texture->GetPath();
        ImGui::SetTooltip("%s\n%ux%u", path.empty() ? label.data() : path.string().c_str(), texture->Width(), texture->Height());
    }
}

void DrawMaterialSourceInfo(std::string_view label, const MaterialSourceInfo& source_info) {
    if (!ImGui::TreeNode(label.data())) return;

    ImGui::Text("Type: %s", MaterialSourceTypeName(source_info.type).data());
    ImGui::TextWrapped("Source path: %s", source_info.source_asset.empty() ? "-" : source_info.source_asset.string().c_str());
    ImGui::TextWrapped("Material path: %s", source_info.source_material_path.empty() ? "-" : source_info.source_material_path.c_str());
    ImGui::TextWrapped("Shader: %s", source_info.source_shader_id.empty() ? "-" : source_info.source_shader_id.c_str());

    if (ImGui::TreeNode("Source Textures")) {
        if (source_info.source_texture_paths.empty()) {
            ImGui::TextUnformatted("-");
        } else {
            for (const auto& path : source_info.source_texture_paths) {
                ImGui::BulletText("%s", path.string().c_str());
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Unsupported Inputs")) {
        if (source_info.unsupported_inputs.empty()) {
            ImGui::TextUnformatted("-");
        } else {
            for (const auto& input : source_info.unsupported_inputs) {
                ImGui::BulletText("%s", input.c_str());
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Unsupported Nodes")) {
        if (source_info.unsupported_nodes.empty()) {
            ImGui::TextUnformatted("-");
        } else {
            for (const auto& node : source_info.unsupported_nodes) {
                ImGui::BulletText("%s", node.c_str());
            }
        }
        ImGui::TreePop();
    }

    ImGui::TreePop();
}

void DrawMaterialPassLayout(Engine& engine, const asset::MaterialPass& pass, const asset::Material& material) {
    std::pmr::string label = "Pass Bindings";
    if (!pass.pass_contract.empty()) {
        label += " [";
        label += pass.pass_contract;
        label += "]";
    }
    if (!ImGui::TreeNode(label.c_str())) return;

    ImGui::Text("Material data: %llu bytes", static_cast<unsigned long long>(pass.material_data.GetDataSize()));

    if (ImGui::TreeNode("Bindings")) {
        const auto textures = ResolvePassTextures(material, pass);
        if (pass.bindings.empty()) {
            ImGui::TextUnformatted("-");
        } else if (ImGui::BeginTable("MaterialPassBindings", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Index");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Bound Texture");
            ImGui::TableHeadersRow();
            for (std::size_t binding_index = 0; binding_index < pass.bindings.size(); ++binding_index) {
                const auto& binding = pass.bindings[binding_index];
                const auto  type    = MaterialPassParameterTypeName(material, binding);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%llu", static_cast<unsigned long long>(binding_index));
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(binding.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(type.data());
                ImGui::TableSetColumnIndex(3);
                if (type == "texture") {
                    const auto texture = binding_index < textures.size() ? textures[binding_index] : nullptr;
                    if (texture) {
                        const auto& path = texture->GetPath();
                        ImGui::TextWrapped("%s", path.empty() ? texture->GetName().data() : path.string().c_str());
                        DrawTextureSlotPreview(engine, binding, texture);
                    } else {
                        ImGui::TextUnformatted("-");
                    }
                } else {
                    ImGui::TextUnformatted("-");
                }
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }

    ImGui::TreePop();
}

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

                    std::pmr::vector<std::shared_ptr<asset::Material>> materials;
                    for (const auto& sub_mesh : mesh->sub_meshes) {
                        if (!sub_mesh.material) continue;
                        const auto duplicated = std::ranges::any_of(materials, [&](const auto& material) {
                            return material.get() == sub_mesh.material.get();
                        });
                        if (!duplicated) materials.emplace_back(sub_mesh.material);
                    }

                    for (const auto& material : materials) {
                        const auto material_parameter_widget = [this, &material](asset::MaterialParameter& parameter) {
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
                                            texture->Load(m_Engine.ResourceLoadContext());
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
                            static asset::Material*                        material_edit_target = nullptr;
                            if (ImGui::IsItemActivated()) {
                                material_edit_start  = parameter;
                                material_edit_target = material.get();
                            }
                            if (edited) {
                                std::visit(
                                    [&](const auto& value) {
                                        material->SetParameter(parameter.name, value);
                                    },
                                    parameter.value);
                            }
                            if (ImGui::IsItemDeactivatedAfterEdit() && material_edit_start && material_edit_target == material.get()) {
                                ExecuteCommand(std::make_unique<MaterialParameterChangeCommand>(
                                    material,
                                    *material_edit_start,
                                    parameter));
                                material_edit_start.reset();
                                material_edit_target = nullptr;
                            }
                        };

                        if (ImGui::TreeNode(material.get(), "%s", material->GetName().data())) {
                            ImGui::Text("Material: %s", material->GetName().data());
                            if (const auto* source_info = m_CookContext.FindMaterialSourceInfo(*material); source_info && HasMaterialSourceInfo(*source_info)) {
                                DrawMaterialSourceInfo("MaterialSourceInfo", *source_info);
                            }
                            if (material->GetPasses().empty()) {
                                ImGui::TextUnformatted("No material passes");
                            } else {
                                for (const auto& pass : material->GetPasses()) {
                                    DrawMaterialPassLayout(m_Engine, pass, *material);
                                }
                            }

                            ImGui::Text("Active Parameters");
                            for (const auto& param : material->GetParameters()) {
                                auto edited_parameter = param;
                                material_parameter_widget(edited_parameter);
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
