export module editor:cook;
import engine;
import :pipeline;
import interop.magic_enum;

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

// Initial rejects nonzero player times. Current explicitly captures the supplied
// local TRS/player state as load-time defaults; neither mode runs simulation.
enum class SceneCookState { Initial,
                            Current };

struct EditorCookOptions {
    std::filesystem::path    asset_root_path;
    const EditorCookContext* context     = nullptr;
    SceneCookState           scene_state = SceneCookState::Initial;
};

// HTGC binary writers, the cooked output consumed by the runtime (see docs/assets/cooked_binary_format.md).
auto CookEditorMaterial(const EditorMaterial& material, const EditorCookOptions& options = {}) -> core::Buffer;
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
    std::pmr::string                                               m_Data;
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
    std::uint64_t             root_offset,
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
                if (texture->GetPath().empty()) {
                    throw std::invalid_argument(std::format(
                        "Texture parameter/default '{}' must be exported before cook", parameter.name));
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

auto BuildShaderRecord(SectionBuilder& meta, StringTableBuilder& strings, const gfx::ShaderDesc& shader, const EditorCookOptions& options) -> asset::ShaderRecord {
    std::vector<asset::ShaderModuleRecord> modules;
    for (const auto& module : shader.program.modules) modules.push_back({
        .name   = strings.Add(module.name),
        .path   = strings.Add(CookedPath(module.path, options.asset_root_path)),
        .source = strings.Add(module.source_code),
    });
    std::vector<asset::StringRef> types;
    for (const auto& type : shader.specialization_types) types.emplace_back(strings.Add(type));
    return {
        .name                 = strings.Add(shader.name),
        .type                 = strings.Add(magic_enum::enum_name(shader.type)),
        .entry                = strings.Add(shader.entry),
        .modules              = meta.AppendArray(std::span<const asset::ShaderModuleRecord>{modules}),
        .specialization_types = meta.AppendArray(std::span<const asset::StringRef>{types}),
    };
}

auto BuildPipelineRecord(SectionBuilder& meta, StringTableBuilder& strings, const gfx::RenderPipelineDesc& pipeline) -> asset::PipelineRecord {
    std::vector<asset::VertexInputRecord> layout;
    for (const auto& attribute : pipeline.vertex_input_layout) layout.push_back({
        .semantic     = strings.Add(attribute.semantic),
        .format       = strings.Add(magic_enum::enum_name(attribute.format)),
        .binding      = attribute.binding,
        .location     = attribute.location,
        .offset       = attribute.offset,
        .stride       = attribute.stride,
        .per_instance = attribute.per_instance ? 1u : 0u,
    });
    return {
        .name                     = strings.Add(pipeline.name),
        .primitive                = strings.Add(magic_enum::enum_name(pipeline.assembly_state.primitive)),
        .cull_mode                = strings.Add(magic_enum::enum_name(pipeline.rasterization_state.cull_mode)),
        .depth_compare            = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_state.depth_compare_op)),
        .render_format            = strings.Add(magic_enum::enum_name(pipeline.render_format)),
        .depth_stencil_format     = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_format)),
        .vertex_input_layout      = meta.AppendArray(std::span<const asset::VertexInputRecord>{layout}),
        .front_counter_clockwise  = pipeline.rasterization_state.front_counter_clockwise ? 1u : 0u,
        .depth_test               = pipeline.depth_stencil_state.depth_test_enable ? 1u : 0u,
        .depth_write              = pipeline.depth_stencil_state.depth_write_enable ? 1u : 0u,
        .restart_enable           = pipeline.assembly_state.restart_enable ? 1u : 0u,
        .fill_mode                = strings.Add(magic_enum::enum_name(pipeline.rasterization_state.fill_mode)),
        .depth_clamp_enable       = pipeline.rasterization_state.depth_clamp_enable ? 1u : 0u,
        .depth_bias_enable        = pipeline.rasterization_state.depth_bias_enable ? 1u : 0u,
        .depth_bias               = pipeline.rasterization_state.depth_bias,
        .depth_bias_clamp         = pipeline.rasterization_state.depth_bias_clamp,
        .depth_bias_slope_factor  = pipeline.rasterization_state.depth_bias_slope_factor,
        .stencil_test_enable      = pipeline.depth_stencil_state.stencil_test_enable ? 1u : 0u,
        .front_fail_op            = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_state.front.fail_op)),
        .front_pass_op            = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_state.front.pass_op)),
        .front_depth_fail_op      = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_state.front.depth_fail_op)),
        .front_compare_op         = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_state.front.compare_op)),
        .front_compare_mask       = pipeline.depth_stencil_state.front.compare_mask,
        .front_write_mask         = pipeline.depth_stencil_state.front.write_mask,
        .front_reference          = pipeline.depth_stencil_state.front.reference,
        .back_fail_op             = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_state.back.fail_op)),
        .back_pass_op             = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_state.back.pass_op)),
        .back_depth_fail_op       = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_state.back.depth_fail_op)),
        .back_compare_op          = strings.Add(magic_enum::enum_name(pipeline.depth_stencil_state.back.compare_op)),
        .back_compare_mask        = pipeline.depth_stencil_state.back.compare_mask,
        .back_write_mask          = pipeline.depth_stencil_state.back.write_mask,
        .back_reference           = pipeline.depth_stencil_state.back.reference,
        .depth_bounds_test_enable = pipeline.depth_stencil_state.depth_bounds_test_enable ? 1u : 0u,
        .depth_bounds             = pipeline.depth_stencil_state.depth_bounds,
        .blend_enable             = pipeline.blend_state.blend_enable ? 1u : 0u,
        .src_color_blend_factor   = strings.Add(magic_enum::enum_name(pipeline.blend_state.src_color_blend_factor)),
        .dst_color_blend_factor   = strings.Add(magic_enum::enum_name(pipeline.blend_state.dst_color_blend_factor)),
        .color_blend_op           = strings.Add(magic_enum::enum_name(pipeline.blend_state.color_blend_op)),
        .src_alpha_blend_factor   = strings.Add(magic_enum::enum_name(pipeline.blend_state.src_alpha_blend_factor)),
        .dst_alpha_blend_factor   = strings.Add(magic_enum::enum_name(pipeline.blend_state.dst_alpha_blend_factor)),
        .alpha_blend_op           = strings.Add(magic_enum::enum_name(pipeline.blend_state.alpha_blend_op)),
        .blend_constants          = pipeline.blend_state.blend_constants,
        .color_write_mask         = static_cast<std::uint32_t>(pipeline.blend_state.color_write_mask),
        .logic_operation_enable   = pipeline.blend_state.logic_operation_enable ? 1u : 0u,
        .logic_op                 = strings.Add(magic_enum::enum_name(pipeline.blend_state.logic_op)),
    };
}

auto BuildPassRecord(SectionBuilder& meta, StringTableBuilder& strings, const asset::MaterialPass& pass, const EditorCookOptions& options) -> asset::PassRecord {
    std::pmr::vector<asset::ShaderRecord> shaders;
    if (pass.pipeline) {
        for (const auto& shader : pass.pipeline->GetShaders()) {
            if (shader) shaders.emplace_back(BuildShaderRecord(meta, strings, shader->GetDesc(), options));
        }
    }

    const auto bindings = pass.bindings |
                          std::views::transform([&](const auto& binding) { return BuildParameterRecord(strings, {.name = binding.name, .value = binding.default_value}, options); }) |
                          std::ranges::to<std::pmr::vector<asset::ParameterRecord>>();

    return {
        .contract     = strings.Add(pass.pass_contract),
        .has_pipeline = pass.pipeline ? 1u : 0u,
        .shaders      = meta.AppendArray(std::span<const asset::ShaderRecord>{shaders}),
        .bindings     = meta.AppendArray(std::span<const asset::ParameterRecord>{bindings}),
        .pipeline     = pass.pipeline ? BuildPipelineRecord(meta, strings, CompilePipelineLayout(pass.pipeline->GetDesc(), pass.pipeline->GetShaders())) : asset::PipelineRecord{},
    };
}

auto BuildMaterialRecord(SectionBuilder& meta, StringTableBuilder& strings, const asset::Material& material, const EditorCookOptions& options) -> asset::MaterialRecord {
    const auto errors = material.ValidateBindings();
    if (!errors.empty()) {
        std::string message = "Material cook validation failed:";
        for (const auto& error : errors) message += std::format("\n{}", error);
        throw std::invalid_argument(message);
    }
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
    SectionBuilder&                                   meta,
    StringTableBuilder&                               strings,
    SectionBuilder&                                   blob,
    const asset::Mesh&                                mesh,
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

    auto              index_type  = asset::IndexType::UINT16;
    std::uint64_t     index_count = 0;
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

auto CookEditorMaterial(const EditorMaterial& material, const EditorCookOptions& options) -> core::Buffer {
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

// A channel is omitted when empty, or reduced to one key when all values are
// identical. Uniform time encoding is used only when it reproduces every double
// exactly. Irregular time arrays are interned across all clips in this scene.
template <typename T>
auto BuildCurveRecord(SectionBuilder& blob, std::map<std::vector<double>, asset::BufferView>& time_arrays,
                      const asset::AnimationCurve<T>& curve, asset::CurveProperty property) -> asset::CurveRecord {
    auto keys = std::span(curve.keys);
    if (std::ranges::all_of(keys, [&](const auto& key) { return key.value == keys.front().value; })) keys = keys.first(1);
    if (keys.size() >= asset::kCookedInvalidIndex) throw std::runtime_error("Too many animation keys");
    std::vector<T>      values;
    std::vector<double> times;
    for (const auto& key : keys) {
        values.emplace_back(key.value);
        times.emplace_back(key.time);
    }
    asset::CurveRecord record{.property      = property,
                              .interpolation = curve.interpolation == asset::AnimationInterpolation::Step ? asset::CurveInterpolation::Step : asset::CurveInterpolation::Linear,
                              .key_count     = static_cast<std::uint32_t>(keys.size()),
                              .time_encoding = asset::CurveTimeEncoding::Uniform,
                              .start         = times.front(),
                              .step          = times.size() > 1 ? times[1] - times[0] : 0};
    for (std::size_t i = 0; i < times.size(); ++i) {
        if (std::fma(double(i), record.step, record.start) != times[i]) {
            record.time_encoding   = asset::CurveTimeEncoding::Explicit;
            auto [entry, inserted] = time_arrays.try_emplace(times);
            if (inserted) entry->second = blob.AppendBytes(std::as_bytes(std::span(times)), 8);
            record.times = entry->second;
            record.start = record.step = 0;
            break;
        }
    }
    static_assert(sizeof(T) == 12 || sizeof(T) == 16);
    record.values = blob.AppendBytes(std::as_bytes(std::span(values)), 4);
    return record;
}

auto CookEditorScene(asset::Scene& scene, EditorCookOptions options) -> core::Buffer {
    // Enumerate storage rather than cached child lists: newly authored entities
    // and direct parent edits must cook without an animation/transform update.
    const auto entities = scene.GetWorld().GetEntityManager().GetEntities();
    if (entities.size() >= asset::kCookedInvalidIndex) throw std::runtime_error("Invalid scene node count");
    std::unordered_map<ecs::Entity, std::uint32_t> node_indices;
    for (std::size_t i = 0; i < entities.size(); ++i) node_indices.emplace(entities[i], static_cast<std::uint32_t>(i));
    const auto node_index = [&](ecs::Entity entity) {
        const auto found = node_indices.find(entity);
        if (found == node_indices.end()) throw std::runtime_error("Scene reference points to a missing or external entity");
        return found->second;
    };
    SectionBuilder                 meta, blob;
    StringTableBuilder             strings;
    std::vector<asset::NodeRecord> nodes;
    for (auto entity : entities) {
        if (entity.Has<physics::CharacterPhysics>())
            throw std::runtime_error("HTGC does not yet serialize runtime physics rigs; use USD import");
        if (!entity.Has<asset::Transform>() || !entity.Has<asset::RelationShip>())
            throw std::runtime_error("Cooked scene nodes require Transform and RelationShip");
        const auto& t = entity.Get<asset::Transform>();
        for (float v : t.position.data)
            if (!std::isfinite(v)) throw std::runtime_error("Non-finite node translation");
        for (float v : t.scaling.data)
            if (!std::isfinite(v)) throw std::runtime_error("Non-finite node scale");
        const float length = t.rotation.norm();
        if (!std::isfinite(length) || std::abs(length - 1) > 1e-3f) throw std::runtime_error("Node rotation must be a unit quaternion");
        const auto parent = entity.Get<asset::RelationShip>().parent;
        nodes.push_back({.name        = strings.Add(EntityName(entity)),
                         .parent      = parent ? node_index(parent) : asset::kCookedInvalidIndex,
                         .translation = t.position,
                         .rotation    = t.rotation,
                         .scale       = t.scaling});
    }
    // Validate a forest: any node may be top-level, but references and cycles are invalid.
    std::vector<unsigned char> marks(nodes.size());
    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
        auto                       cursor = i;
        std::vector<std::uint32_t> path;
        while (cursor < nodes.size() && marks[cursor] == 0) {
            marks[cursor] = 1;
            path.emplace_back(cursor);
            cursor = nodes[cursor].parent;
        }
        if (cursor != asset::kCookedInvalidIndex && (cursor >= nodes.size() || marks[cursor] == 1))
            throw std::runtime_error("Invalid or cyclic scene hierarchy");
        for (auto node : path) marks[node] = 2;
    }
    SceneCookAssets                                          assets;
    std::vector<std::shared_ptr<const asset::Skin>>          skin_assets;
    std::vector<std::shared_ptr<const asset::AnimationClip>> clip_assets;
    const auto                                               intern = []<typename T>(auto& list, const T& value) -> std::uint32_t {
        if (!value) throw std::runtime_error("Null scene asset");
        auto found = std::ranges::find(list, value);
        if (found != list.end()) return static_cast<std::uint32_t>(found - list.begin());
        if (list.size() >= asset::kCookedInvalidIndex) throw std::runtime_error("Too many scene assets");
        list.emplace_back(value);
        return static_cast<std::uint32_t>(list.size() - 1);
    };
    std::vector<asset::MeshInstanceRecord>    instances;
    std::vector<asset::CameraRecord>          cameras;
    std::vector<asset::LightRecord>           lights;
    std::vector<asset::AnimationPlayerRecord> players;
    std::vector<asset::AnimationTargetRecord> targets;
    std::vector<asset::SkinnedMeshRecord>     skinned_meshes;
    for (auto entity : entities) {
        const auto node = node_index(entity);
        if (entity.Has<asset::MeshComponent>()) {
            const auto mesh = entity.Get<asset::MeshComponent>().mesh;
            const auto mesh_index = intern(assets.meshes, mesh);
            const auto& overrides = entity.Get<asset::MeshComponent>().material_overrides;
            if (overrides.size() > mesh->sub_meshes.size()) throw std::runtime_error("Too many material overrides");
            std::vector<std::uint32_t> material_indices;
            for (const auto& material : overrides)
                material_indices.push_back(material ? intern(assets.materials, material) : asset::kCookedInvalidIndex);
            instances.push_back({.node = node, .mesh_index = mesh_index, .material_overrides = meta.AppendArray(std::span<const std::uint32_t>(material_indices))});
            for (const auto& submesh : mesh->sub_meshes)
                if (submesh.material) intern(assets.materials, submesh.material);
        }
        if (entity.Has<asset::CameraComponent>()) {
            const auto& p = entity.Get<asset::CameraComponent>();
            cameras.push_back({.node = node, .aspect = p.aspect, .near_clip = p.near_clip, .far_clip = p.far_clip, .horizontal_fov = p.horizontal_fov});
        }
        if (entity.Has<asset::LightComponent>()) {
            const auto& p = entity.Get<asset::LightComponent>();
            lights.push_back({.type = strings.Add(magic_enum::enum_name(p.type)), .node = node, .intensity = p.intensity, .color = p.color, .inner_cone_angle = p.inner_cone_angle, .outer_cone_angle = p.outer_cone_angle});
        }
        if (entity.Has<asset::AnimationPlayer>()) {
            const auto& p = entity.Get<asset::AnimationPlayer>();
            if (!std::isfinite(p.time) || !std::isfinite(p.speed)) throw std::runtime_error("Invalid animation player state");
            if (options.scene_state == SceneCookState::Initial && p.time != 0)
                throw std::runtime_error("Initial scene cook requires time zero; use SceneCookState::Current to capture a preview state explicitly");
            players.push_back({.node = node, .clip = p.clip ? intern(clip_assets, p.clip) : asset::kCookedInvalidIndex, .time = p.time, .speed = p.speed, .looping = p.looping, .playing = p.playing});
        }
        if (entity.Has<asset::AnimationTarget>()) {
            const auto& target = entity.Get<asset::AnimationTarget>();
            const auto  player = node_index(target.player);
            if (!target.player.Has<asset::AnimationPlayer>()) throw std::runtime_error("Animation target has no player");
            targets.push_back({.node = node, .player_node = player, .target = target.target});
        }
        if (entity.Has<asset::SkinnedMesh>()) {
            const auto& skin = entity.Get<asset::SkinnedMesh>();
            if (!entity.Has<asset::MeshComponent>() || !skin.skin || skin.joints.size() != skin.skin->GetInverseBindMatrices().size())
                throw std::runtime_error("Invalid skin binding");
            if (!entity.Get<asset::MeshComponent>().mesh->vertices) throw std::runtime_error("Skinned mesh has no vertices");
            const auto& vertices = *entity.Get<asset::MeshComponent>().mesh->vertices;
            const auto  indices  = vertices.Span<asset::VertexAttribute::BlendIndex>();
            const auto  weights  = vertices.Span<asset::VertexAttribute::BlendWeight>();
            if (vertices.Span<asset::VertexAttribute::Position>().size() != vertices.Size() || indices.size() != vertices.Size() || weights.size() != vertices.Size()) throw std::runtime_error("Missing skin influences");
            for (std::size_t v = 0; v < vertices.Size(); ++v) {
                for (unsigned k = 0; k < 4; ++k) {
                    const float w = weights[v][k];
                    if (!std::isfinite(w) || w < 0 || (w > 0 && indices[v][k] >= skin.joints.size())) throw std::runtime_error("Invalid skin influence");
                }
            }
            std::vector<std::uint32_t> joints;
            for (auto joint : skin.joints) joints.emplace_back(node_index(joint));
            skinned_meshes.push_back({.node = node, .skin = intern(skin_assets, skin.skin), .joints = meta.AppendArray(std::span<const std::uint32_t>(joints))});
        }
    }
    std::vector<asset::MaterialRecord> materials;
    for (const auto& material : assets.materials) materials.emplace_back(BuildMaterialRecord(meta, strings, *material, options));
    std::vector<asset::MeshRecord> meshes;
    for (const auto& mesh : assets.meshes) meshes.emplace_back(BuildMeshRecord(meta, strings, blob, *mesh, assets.materials));
    std::vector<asset::SkinRecord> skins;
    for (const auto& skin : skin_assets) {
        std::vector<asset::AffineMatrixRecord> matrices;
        for (const auto& matrix : skin->GetInverseBindMatrices()) {
            if (matrix[3] != math::vec4f(0, 0, 0, 1)) throw std::runtime_error("Cannot encode a non-affine skin matrix");
            asset::AffineMatrixRecord packed;
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 4; ++column)
                    packed.values[row * 4 + column] = matrix[row][column];
            matrices.emplace_back(packed);
        }
        skins.push_back({.name = strings.Add(skin->GetName()), .inverse_bind_matrices = blob.AppendBytes(std::as_bytes(std::span(matrices)), 16)});
    }
    std::map<std::vector<double>, asset::BufferView> time_arrays;
    std::vector<asset::AnimationClipRecord>          clips;
    for (const auto& clip : clip_assets) {
        std::vector<asset::AnimationTrackRecord> tracks;
        for (const auto& track : clip->GetTracks()) {
            std::vector<asset::CurveRecord> curves;
            if (!track.positions.keys.empty()) curves.emplace_back(BuildCurveRecord(blob, time_arrays, track.positions, asset::CurveProperty::Translation));
            if (!track.rotations.keys.empty()) curves.emplace_back(BuildCurveRecord(blob, time_arrays, track.rotations, asset::CurveProperty::Rotation));
            if (!track.scales.keys.empty()) curves.emplace_back(BuildCurveRecord(blob, time_arrays, track.scales, asset::CurveProperty::Scale));
            tracks.push_back({.target = track.target, .curves = meta.AppendArray(std::span<const asset::CurveRecord>(curves))});
        }
        clips.push_back({.name = strings.Add(clip->GetName()), .duration = clip->GetDuration(), .tracks = meta.AppendArray(std::span<const asset::AnimationTrackRecord>(tracks))});
    }
    const asset::SceneRecord root{
        .name           = strings.Add(scene.GetName()),
        .materials      = meta.AppendArray(std::span<const asset::MaterialRecord>(materials)),
        .meshes         = meta.AppendArray(std::span<const asset::MeshRecord>(meshes)),
        .nodes          = meta.AppendArray(std::span<const asset::NodeRecord>(nodes)),
        .instances      = meta.AppendArray(std::span<const asset::MeshInstanceRecord>(instances)),
        .cameras        = meta.AppendArray(std::span<const asset::CameraRecord>(cameras)),
        .lights         = meta.AppendArray(std::span<const asset::LightRecord>(lights)),
        .skins          = meta.AppendArray(std::span<const asset::SkinRecord>(skins)),
        .clips          = meta.AppendArray(std::span<const asset::AnimationClipRecord>(clips)),
        .players        = meta.AppendArray(std::span<const asset::AnimationPlayerRecord>(players)),
        .targets        = meta.AppendArray(std::span<const asset::AnimationTargetRecord>(targets)),
        .skinned_meshes = meta.AppendArray(std::span<const asset::SkinnedMeshRecord>(skinned_meshes)),
    };
    return AssembleCookedFile(asset::CookedAssetType::Scene, meta.Append(root), meta, strings, blob);
}

}  // namespace hitagi
