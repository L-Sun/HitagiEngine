module;

export module editor:cook;
import engine;
import magic_enum;

export namespace hitagi {

enum struct MaterialSourceType : std::uint8_t {
    Builtin,
    Imported,
    MDL,
    JSON,
    Generated,
    Unknown,
};

struct MaterialSourceParameter {
    std::pmr::string name;
    std::pmr::string value;

    inline bool operator==(const MaterialSourceParameter&) const noexcept = default;
};

struct MaterialSourceInfo {
    MaterialSourceType                        type = MaterialSourceType::Unknown;
    std::filesystem::path                     source_asset;
    std::string                               source_material_path;
    std::string                               source_shader_id;
    std::pmr::vector<std::filesystem::path>   source_texture_paths;
    std::pmr::vector<MaterialSourceParameter> source_parameters;
    std::pmr::vector<std::string>             unsupported_inputs;
    std::pmr::vector<std::string>             unsupported_nodes;

    inline bool operator==(const MaterialSourceInfo&) const noexcept = default;
};

struct EditorMaterial {
    std::shared_ptr<asset::Material> material;
    MaterialSourceInfo               source_info;
};

class EditorCookContext {
public:
    void Clear() noexcept;
    void SetMaterialSourceInfo(const std::shared_ptr<asset::Material>& material, MaterialSourceInfo source_info);
    auto FindMaterialSourceInfo(const asset::Material& material) const noexcept -> const MaterialSourceInfo*;
    auto MakeEditorMaterial(const std::shared_ptr<asset::Material>& material) const -> EditorMaterial;

private:
    std::pmr::unordered_map<const asset::Material*, MaterialSourceInfo> m_MaterialSourceInfo;
};

struct EditorCookOptions {
    std::filesystem::path    asset_root_path;
    const EditorCookContext* context = nullptr;
};

// HTGC binary writers, the cooked output consumed by the runtime (see docs/assets/cooked_binary_format.md).
auto CookEditorMaterial(const EditorMaterial& material, EditorCookOptions options = {}) -> core::Buffer;
auto CookEditorScene(asset::Scene& scene, EditorCookOptions options = {}) -> core::Buffer;

}  // namespace hitagi

// Non-exported helpers below have module linkage: internal to the editor
// module, invisible to importers.
namespace hitagi {

using namespace math;

auto CookedPath(const std::filesystem::path& path, const std::filesystem::path& root) -> std::string {
    if (path.empty()) return {};
    if (root.empty()) return path.generic_string();

    std::error_code error;
    const auto      absolute_path = std::filesystem::absolute(path, error).lexically_normal();
    if (error) return path.generic_string();
    const auto absolute_root = std::filesystem::absolute(root, error).lexically_normal();
    if (error) return path.generic_string();

    const auto relative_path = std::filesystem::relative(absolute_path, absolute_root, error);
    if (!error && !relative_path.empty() && *relative_path.begin() != std::filesystem::path("..")) {
        if (relative_path == ".") return {};
        return relative_path.generic_string();
    }
    return path.generic_string();
}

auto ParameterTypeName(const asset::MaterialParameterValue& value) -> std::string {
    return std::visit(
        utils::Overloaded{
            [](const float&) { return std::string("Float"); },
            [](const std::int32_t&) { return std::string("Int32"); },
            [](const std::uint32_t&) { return std::string("UInt32"); },
            [](const vec2i&) { return std::string("Vec2i"); },
            [](const vec2u&) { return std::string("Vec2u"); },
            [](const vec2f&) { return std::string("Vec2f"); },
            [](const vec3i&) { return std::string("Vec3i"); },
            [](const vec3u&) { return std::string("Vec3u"); },
            [](const vec3f&) { return std::string("Vec3f"); },
            [](const vec4i&) { return std::string("Vec4i"); },
            [](const vec4u&) { return std::string("Vec4u"); },
            [](const vec4f&) { return std::string("Vec4f"); },
            [](const Color&) { return std::string("Color"); },
            [](const mat4f&) { return std::string("Mat4f"); },
            [](const std::shared_ptr<asset::Texture>&) { return std::string("Texture2D"); },
        },
        value);
}

auto EntityName(ecs::Entity entity) -> std::string {
    if (entity && entity.Has<asset::MetaInfo>()) return std::string(entity.Get<asset::MetaInfo>().name);
    return entity ? std::format("Entity {}", entity.GetId()) : std::string{};
}

struct SceneCookAssets {
    std::pmr::vector<std::shared_ptr<asset::Mesh>>     meshes;
    std::pmr::vector<std::shared_ptr<asset::Material>> materials;
};

auto CollectSceneAssets(asset::Scene& scene) -> SceneCookAssets {
    SceneCookAssets assets;
    for (const auto entity : scene.GetMeshEntities()) {
        if (!entity || !entity.Has<asset::MeshComponent>()) continue;
        const auto mesh = entity.Get<asset::MeshComponent>().mesh;
        if (!mesh || std::ranges::contains(assets.meshes, mesh)) continue;
        assets.meshes.emplace_back(mesh);
        for (const auto& sub_mesh : mesh->sub_meshes) {
            if (sub_mesh.material && !std::ranges::contains(assets.materials, sub_mesh.material)) {
                assets.materials.emplace_back(sub_mesh.material);
            }
        }
    }
    return assets;
}

// --- HTGC binary writer -------------------------------------------------------

class StringTableBuilder {
public:
    auto Add(std::string_view value) -> asset::StringRef {
        if (value.empty()) return {};
        if (const auto iter = m_Lookup.find(value); iter != m_Lookup.end()) return iter->second;
        const asset::StringRef ref{
            .offset = static_cast<std::uint32_t>(m_Data.size()),
            .length = static_cast<std::uint32_t>(value.size()),
        };
        m_Data.append(value);
        m_Lookup.emplace(std::pmr::string(value), ref);
        return ref;
    }

    auto Data() const noexcept -> std::span<const std::byte> {
        return std::as_bytes(std::span{m_Data.data(), m_Data.size()});
    }

private:
    std::pmr::string                                          m_Data;
    std::pmr::map<std::pmr::string, asset::StringRef, std::less<>> m_Lookup;
};

class SectionBuilder {
public:
    void Align(std::size_t alignment) {
        m_Data.resize((m_Data.size() + alignment - 1) / alignment * alignment);
    }

    template <asset::CookedRecord T>
    auto Append(const T& record) -> std::uint64_t {
        Align(asset::kCookedRecordAlignment);
        const auto offset = m_Data.size();
        AppendRaw(&record, sizeof(T));
        return offset;
    }

    template <asset::CookedRecord T>
    auto AppendArray(std::span<const T> records) -> asset::ArrayRef {
        Align(asset::kCookedRecordAlignment);
        const asset::ArrayRef ref{.offset = m_Data.size(), .count = records.size()};
        AppendRaw(records.data(), records.size_bytes());
        return ref;
    }

    auto AppendBytes(std::span<const std::byte> bytes, std::size_t alignment) -> asset::BufferView {
        Align(alignment);
        const asset::BufferView view{.offset = m_Data.size(), .size = bytes.size()};
        AppendRaw(bytes.data(), bytes.size());
        return view;
    }

    auto Data() const noexcept -> std::span<const std::byte> { return m_Data; }

private:
    void AppendRaw(const void* data, std::size_t size) {
        if (size == 0) return;
        const auto offset = m_Data.size();
        m_Data.resize(offset + size);
        std::memcpy(m_Data.data() + offset, data, size);
    }

    std::pmr::vector<std::byte> m_Data;
};

auto AssembleCookedFile(
    asset::CookedAssetType    asset_type,
    std::uint64_t        root_offset,
    const SectionBuilder&     meta,
    const StringTableBuilder& strings,
    const SectionBuilder&     blob) -> core::Buffer {
    constexpr auto align_up = [](std::uint64_t value, std::uint64_t alignment) {
        return (value + alignment - 1) / alignment * alignment;
    };

    const std::uint64_t meta_offset    = sizeof(asset::CookedFileHeader);
    const std::uint64_t strings_offset = meta_offset + meta.Data().size();
    const std::uint64_t blob_offset    = align_up(strings_offset + strings.Data().size(), asset::kCookedBlobAlignment);

    const asset::CookedFileHeader header{
        .magic       = asset::kCookedMagic,
        .version     = asset::kCookedVersion,
        .asset_type  = asset_type,
        .meta        = {.offset = meta_offset, .size = meta.Data().size()},
        .strings     = {.offset = strings_offset, .size = strings.Data().size()},
        .blob        = {.offset = blob_offset, .size = blob.Data().size()},
        .root_offset = root_offset,
    };

    core::Buffer buffer(blob_offset + blob.Data().size());
    std::ranges::fill(buffer.Span<std::byte>(), std::byte{});
    const auto write = [&](std::uint64_t offset, const void* data, std::size_t size) {
        if (size != 0) std::memcpy(buffer.GetData() + offset, data, size);
    };
    write(0, &header, sizeof(header));
    write(meta_offset, meta.Data().data(), meta.Data().size());
    write(strings_offset, strings.Data().data(), strings.Data().size());
    write(blob_offset, blob.Data().data(), blob.Data().size());
    return buffer;
}

auto BuildParameterRecord(StringTableBuilder& strings, const asset::MaterialParameter& parameter, const EditorCookOptions& options) -> asset::ParameterRecord {
    asset::ParameterRecord record{
        .name = strings.Add(parameter.name),
        .type = strings.Add(ParameterTypeName(parameter.value)),
    };
    std::visit(
        utils::Overloaded{
            [&](const std::shared_ptr<asset::Texture>& texture) {
                if (!texture) {
                    record.flags |= asset::kCookedParameterNullTexture;
                    return;
                }
                record.texture_name = strings.Add(texture->GetName());
                record.texture_path = strings.Add(CookedPath(texture->GetPath(), options.asset_root_path));
            },
            [&](const auto& value) {
                static_assert(sizeof(value) <= asset::kCookedParameterPayloadSize);
                std::memcpy(record.payload.data(), std::addressof(value), sizeof(value));
            },
        },
        parameter.value);
    return record;
}

auto BuildShaderRecord(StringTableBuilder& strings, const gfx::ShaderDesc& shader, const EditorCookOptions& options) -> asset::ShaderRecord {
    return {
        .name   = strings.Add(shader.name),
        .type   = strings.Add(magic_enum::enum_name(shader.type)),
        .entry  = strings.Add(shader.entry),
        .path   = strings.Add(CookedPath(shader.path, options.asset_root_path)),
        .source = strings.Add(shader.source_code),
    };
}

auto BuildPipelineRecord(StringTableBuilder& strings, const gfx::RenderPipelineDesc& pipeline) -> asset::PipelineRecord {
    return {
        .name                    = strings.Add(pipeline.name),
        .primitive               = strings.Add(magic_enum::enum_name(pipeline.assembly_state.primitive)),
        .cull_mode               = strings.Add(magic_enum::enum_name(pipeline.rasterization_state.cull_mode)),
        .depth_compare           = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_state.depth_compare_op)),
        .render_format           = strings.Add(magic_enum::enum_name(pipeline.render_format)),
        .depth_stencil_format    = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_format)),
        .front_counter_clockwise = pipeline.rasterization_state.front_counter_clockwise ? 1u : 0u,
        .depth_test              = pipeline.depth_stencil_state.depth_test_enable ? 1u : 0u,
        .depth_write             = pipeline.depth_stencil_state.depth_write_enable ? 1u : 0u,
    };
}

auto BuildPassRecord(SectionBuilder& meta, StringTableBuilder& strings, const asset::MaterialPass& pass, const EditorCookOptions& options) -> asset::PassRecord {
    std::pmr::vector<asset::ShaderRecord> shaders;
    if (pass.pipeline) {
        for (const auto& shader : pass.pipeline->GetShaders()) {
            if (shader) shaders.emplace_back(BuildShaderRecord(strings, shader->GetDesc(), options));
        }
    }

    const auto bindings = pass.bindings |
                          std::views::transform([&](const auto& binding) { return strings.Add(binding); }) |
                          std::ranges::to<std::pmr::vector<asset::StringRef>>();

    return {
        .contract     = strings.Add(pass.pass_contract),
        .has_pipeline = pass.pipeline ? 1u : 0u,
        .shaders      = meta.AppendArray(std::span<const asset::ShaderRecord>{shaders}),
        .bindings     = meta.AppendArray(std::span<const asset::StringRef>{bindings}),
        .pipeline     = pass.pipeline ? BuildPipelineRecord(strings, pass.pipeline->GetDesc()) : asset::PipelineRecord{},
    };
}

auto BuildMaterialRecord(SectionBuilder& meta, StringTableBuilder& strings, const asset::Material& material, const EditorCookOptions& options) -> asset::MaterialRecord {
    const auto parameters = material.GetParameters() |
                            std::views::transform([&](const auto& parameter) { return BuildParameterRecord(strings, parameter, options); }) |
                            std::ranges::to<std::pmr::vector<asset::ParameterRecord>>();

    const auto passes = material.GetPasses() |
                        std::views::transform([&](const auto& pass) { return BuildPassRecord(meta, strings, pass, options); }) |
                        std::ranges::to<std::pmr::vector<asset::PassRecord>>();

    return {
        .name       = strings.Add(material.GetName()),
        .parameters = meta.AppendArray(std::span<const asset::ParameterRecord>{parameters}),
        .passes     = meta.AppendArray(std::span<const asset::PassRecord>{passes}),
    };
}

auto BuildMeshRecord(
    SectionBuilder&                                    meta,
    StringTableBuilder&                                strings,
    SectionBuilder&                                    blob,
    const asset::Mesh&                                 mesh,
    std::span<const std::shared_ptr<asset::Material>> materials) -> asset::MeshRecord {
    std::pmr::vector<asset::VertexAttributeRecord> attributes;
    if (mesh.vertices) {
        magic_enum::enum_for_each<asset::VertexAttribute>([&](auto attr) {
            constexpr asset::VertexAttribute Attr = attr;
            const auto                       data = std::as_const(*mesh.vertices).Span<Attr>();
            if (data.empty()) return;
            attributes.emplace_back(asset::VertexAttributeRecord{
                .attribute = strings.Add(magic_enum::enum_name(Attr)),
                .data      = blob.AppendBytes(std::as_bytes(data), asset::kCookedBlobAlignment),
            });
        });
    }

    auto               index_type  = asset::IndexType::UINT16;
    std::uint64_t      index_count = 0;
    asset::BufferView index_view{};
    if (mesh.indices) {
        index_type  = mesh.indices->Type();
        index_count = mesh.indices->Size();
        index_view  = index_type == asset::IndexType::UINT16
                          ? blob.AppendBytes(std::as_bytes(std::as_const(*mesh.indices).Span<asset::IndexType::UINT16>()), asset::kCookedBlobAlignment)
                          : blob.AppendBytes(std::as_bytes(std::as_const(*mesh.indices).Span<asset::IndexType::UINT32>()), asset::kCookedBlobAlignment);
    }

    const auto sub_meshes = mesh.sub_meshes |
                            std::views::transform([&](const asset::Mesh::SubMesh& sub_mesh) {
                                const auto iter = std::ranges::find(materials, sub_mesh.material);
                                return asset::SubMeshRecord{
                                    .index_count    = sub_mesh.index_count,
                                    .index_offset   = sub_mesh.index_offset,
                                    .vertex_offset  = sub_mesh.vertex_offset,
                                    .material_index = iter != materials.end()
                                                          ? static_cast<std::uint32_t>(std::ranges::distance(materials.begin(), iter))
                                                          : asset::kCookedInvalidIndex,
                                };
                            }) |
                            std::ranges::to<std::pmr::vector<asset::SubMeshRecord>>();

    return {
        .name         = strings.Add(mesh.GetName()),
        .vertex_count = mesh.vertices ? mesh.vertices->Size() : 0,
        .attributes   = meta.AppendArray(std::span<const asset::VertexAttributeRecord>{attributes}),
        .index_type   = strings.Add(magic_enum::enum_name(index_type)),
        .index_count  = index_count,
        .indices      = index_view,
        .sub_meshes   = meta.AppendArray(std::span<const asset::SubMeshRecord>{sub_meshes}),
        .aabb_min     = mesh.aabb.min_point,
        .aabb_max     = mesh.aabb.max_point,
    };
}

void EditorCookContext::Clear() noexcept {
    m_MaterialSourceInfo.clear();
}

void EditorCookContext::SetMaterialSourceInfo(const std::shared_ptr<asset::Material>& material, MaterialSourceInfo source_info) {
    if (!material) return;
    m_MaterialSourceInfo.insert_or_assign(material.get(), std::move(source_info));
}

auto EditorCookContext::FindMaterialSourceInfo(const asset::Material& material) const noexcept -> const MaterialSourceInfo* {
    const auto iter = m_MaterialSourceInfo.find(std::addressof(material));
    return iter == m_MaterialSourceInfo.end() ? nullptr : std::addressof(iter->second);
}

auto EditorCookContext::MakeEditorMaterial(const std::shared_ptr<asset::Material>& material) const -> EditorMaterial {
    EditorMaterial result{.material = material};
    if (material) {
        if (const auto* source_info = FindMaterialSourceInfo(*material)) result.source_info = *source_info;
    }
    return result;
}

auto CookEditorMaterial(const EditorMaterial& material, EditorCookOptions options) -> core::Buffer {
    if (!material.material) {
        throw std::runtime_error("Cannot cook an empty editor material.");
    }

    SectionBuilder     meta;
    SectionBuilder     blob;
    StringTableBuilder strings;

    const auto record      = BuildMaterialRecord(meta, strings, *material.material, options);
    const auto root_offset = meta.Append(record);
    return AssembleCookedFile(asset::CookedAssetType::Material, root_offset, meta, strings, blob);
}

auto CookEditorScene(asset::Scene& scene, EditorCookOptions options) -> core::Buffer {
    scene.Update();

    const auto scene_assets = CollectSceneAssets(scene);

    SectionBuilder     meta;
    SectionBuilder     blob;
    StringTableBuilder strings;

    const auto materials = scene_assets.materials |
                           std::views::transform([&](const auto& material) { return BuildMaterialRecord(meta, strings, *material, options); }) |
                           std::ranges::to<std::pmr::vector<asset::MaterialRecord>>();

    const auto meshes = scene_assets.meshes |
                        std::views::transform([&](const auto& mesh) { return BuildMeshRecord(meta, strings, blob, *mesh, scene_assets.materials); }) |
                        std::ranges::to<std::pmr::vector<asset::MeshRecord>>();

    std::pmr::vector<asset::MeshInstanceRecord> instances;
    for (const auto entity : scene.GetMeshEntities()) {
        if (!entity || !entity.Has<asset::MeshComponent>() || !entity.Has<asset::Transform>()) continue;
        const auto iter = std::ranges::find(scene_assets.meshes, entity.Get<asset::MeshComponent>().mesh);
        if (iter == scene_assets.meshes.end()) continue;
        instances.emplace_back(asset::MeshInstanceRecord{
            .name       = strings.Add(EntityName(entity)),
            .mesh_index = static_cast<std::uint32_t>(std::ranges::distance(scene_assets.meshes.begin(), iter)),
            .transform  = entity.Get<asset::Transform>().world_matrix,
        });
    }

    std::pmr::vector<asset::CameraRecord> cameras;
    for (const auto entity : scene.GetCameraEntities()) {
        if (!entity || !entity.Has<asset::CameraComponent>() || !entity.Has<asset::Transform>()) continue;
        const auto camera = entity.Get<asset::CameraComponent>().camera;
        if (!camera) continue;
        cameras.emplace_back(asset::CameraRecord{
            .name           = strings.Add(EntityName(entity)),
            .transform      = entity.Get<asset::Transform>().world_matrix,
            .aspect         = camera->parameters.aspect,
            .near_clip      = camera->parameters.near_clip,
            .far_clip       = camera->parameters.far_clip,
            .horizontal_fov = camera->parameters.horizontal_fov,
            .eye            = camera->parameters.eye,
            .look_dir       = camera->parameters.look_dir,
            .up             = camera->parameters.up,
        });
    }

    std::pmr::vector<asset::LightRecord> lights;
    for (const auto entity : scene.GetLightEntities()) {
        if (!entity || !entity.Has<asset::LightComponent>() || !entity.Has<asset::Transform>()) continue;
        const auto light = entity.Get<asset::LightComponent>().light;
        if (!light) continue;
        lights.emplace_back(asset::LightRecord{
            .name             = strings.Add(EntityName(entity)),
            .type             = strings.Add(magic_enum::enum_name(light->parameters.type)),
            .transform        = entity.Get<asset::Transform>().world_matrix,
            .intensity        = light->parameters.intensity,
            .color            = light->parameters.color,
            .position         = light->parameters.position,
            .direction        = light->parameters.direction,
            .up               = light->parameters.up,
            .inner_cone_angle = light->parameters.inner_cone_angle,
            .outer_cone_angle = light->parameters.outer_cone_angle,
        });
    }

    const asset::SceneRecord root{
        .name      = strings.Add(scene.GetName()),
        .materials = meta.AppendArray(std::span<const asset::MaterialRecord>{materials}),
        .meshes    = meta.AppendArray(std::span<const asset::MeshRecord>{meshes}),
        .instances = meta.AppendArray(std::span<const asset::MeshInstanceRecord>{instances}),
        .cameras   = meta.AppendArray(std::span<const asset::CameraRecord>{cameras}),
        .lights    = meta.AppendArray(std::span<const asset::LightRecord>{lights}),
    };
    const auto root_offset = meta.Append(root);
    return AssembleCookedFile(asset::CookedAssetType::Scene, root_offset, meta, strings, blob);
}

}  // namespace hitagi
