module;

// Implementation partition (not exported from the module interface): the cooked
// parsers stay internal to hitagi::asset. Module implementation units such as
// asset_manager.cpp import :cooked_binary to use these declarations.
module asset:cooked_binary;
import std;
import core;
import gfx;
import math;
import ecs;
import interop.magic_enum;
import :cooked_format;
import :shader;
import :pipeline;
import :texture;
import :material;
import :mesh;
import :camera;
import :light;
import :scene;
import :transform;
import :animation;
import :skin;

using namespace hitagi::math;

namespace hitagi::asset {

// Injected by AssetManager to create/deduplicate texture instances by path. It is
// required: only the manager knows how a lazily loaded texture reads its pixels
// and where its decode runs.
using CookedTextureResolver = std::function<std::shared_ptr<Texture>(const std::filesystem::path& path, std::string_view name)>;

auto ResolveCookedPath(const std::filesystem::path& path, const std::filesystem::path& root) -> std::filesystem::path {
    if (path.empty() || path.is_absolute() || root.empty()) return path;
    if (std::filesystem::exists(path)) return path;
    return root / path;
}

class CookedReader {
public:
    CookedReader(std::span<const std::byte> file, CookedAssetType expected_type) {
        if (file.size() < sizeof(CookedFileHeader)) {
            throw std::runtime_error("Cooked binary is truncated.");
        }
        m_Header = ReadFrom<CookedFileHeader>(file, 0);
        if (!std::ranges::equal(std::span{m_Header.magic}, kCookedMagic)) {
            throw std::runtime_error("Cooked binary has an invalid magic.");
        }
        if (m_Header.version != kCookedVersion) {
            throw std::runtime_error(std::format(
                "Cooked binary version mismatch (file: {}, engine: {}); re-cook the asset.",
                m_Header.version, kCookedVersion));
        }
        if (m_Header.asset_type != expected_type) {
            throw std::runtime_error(std::format(
                "Cooked binary asset type mismatch (file: {}, expected: {}).",
                magic_enum::enum_name(m_Header.asset_type), magic_enum::enum_name(expected_type)));
        }
        m_Meta    = Slice(file, m_Header.meta);
        m_Strings = Slice(file, m_Header.strings);
        m_Blob    = Slice(file, m_Header.blob);
    }

    auto GetHeader() const noexcept -> const CookedFileHeader& { return m_Header; }

    // Records are memcpy'd out instead of viewed in place: this sidesteps any
    // alignment concern and records are small, so the copies are negligible.
    template <CookedRecord T>
    auto Read(std::uint64_t offset) const -> T {
        return ReadFrom<T>(m_Meta, offset);
    }

    template <CookedRecord T>
    auto ReadArray(ArrayRef ref) const {
        if (ref.offset > m_Meta.size() || ref.count > (m_Meta.size() - ref.offset) / sizeof(T))
            throw std::runtime_error("Cooked array reference is out of range");
        return std::views::iota(std::uint64_t{0}, ref.count) |
               std::views::transform([this, ref](std::uint64_t index) {
                   return Read<T>(ref.offset + index * sizeof(T));
               });
    }

    auto GetString(StringRef ref) const -> std::string_view {
        if (ref.length == 0) return {};
        CheckRange(m_Strings, ref.offset, ref.length);
        return {reinterpret_cast<const char*>(m_Strings.data() + ref.offset), ref.length};
    }

    auto GetBlob(BufferView view) const -> std::span<const std::byte> {
        CheckRange(m_Blob, view.offset, view.size);
        return m_Blob.subspan(view.offset, view.size);
    }

private:
    static void CheckRange(std::span<const std::byte> section, std::uint64_t offset, std::uint64_t size) {
        if (offset > section.size() || size > section.size() - offset) {
            throw std::runtime_error("Cooked binary reference is out of range.");
        }
    }

    template <CookedRecord T>
    static auto ReadFrom(std::span<const std::byte> section, std::uint64_t offset) -> T {
        CheckRange(section, offset, sizeof(T));
        T value;
        std::memcpy(&value, section.data() + offset, sizeof(T));
        return value;
    }

    static auto Slice(std::span<const std::byte> file, BufferView view) -> std::span<const std::byte> {
        CheckRange(file, view.offset, view.size);
        return file.subspan(view.offset, view.size);
    }

    CookedFileHeader           m_Header{};
    std::span<const std::byte> m_Meta;
    std::span<const std::byte> m_Strings;
    std::span<const std::byte> m_Blob;
};

template <typename T>
auto PayloadAs(const ParameterRecord& record) -> T {
    static_assert(sizeof(T) <= kCookedParameterPayloadSize);
    T value;
    std::memcpy(&value, record.payload.data(), sizeof(T));
    return value;
}

auto ParameterValueFromRecord(const CookedReader& reader, const ParameterRecord& record, const std::filesystem::path& asset_root_path, const CookedTextureResolver& texture_resolver) -> MaterialParameterValue {
    const auto type = reader.GetString(record.type);
    if (type == "Float") return PayloadAs<float>(record);
    if (type == "Int32") return PayloadAs<std::int32_t>(record);
    if (type == "UInt32") return PayloadAs<std::uint32_t>(record);
    if (type == "Vec2i") return PayloadAs<vec2i>(record);
    if (type == "Vec2u") return PayloadAs<vec2u>(record);
    if (type == "Vec2f") return PayloadAs<vec2f>(record);
    if (type == "Vec3i") return PayloadAs<vec3i>(record);
    if (type == "Vec3u") return PayloadAs<vec3u>(record);
    if (type == "Vec3f") return PayloadAs<vec3f>(record);
    if (type == "Vec4i") return PayloadAs<vec4i>(record);
    if (type == "Vec4u") return PayloadAs<vec4u>(record);
    if (type == "Vec4f") return PayloadAs<vec4f>(record);
    if (type == "Color") return PayloadAs<Color>(record);
    if (type == "Mat4f") return PayloadAs<mat4f>(record);
    if (type == "Texture2D") {
        if (record.flags & kCookedParameterNullTexture) return std::shared_ptr<Texture>{};
        const auto path = ResolveCookedPath(reader.GetString(record.texture_path), asset_root_path);
        const auto name = reader.GetString(record.texture_name);
        if (!texture_resolver) {
            throw std::invalid_argument("Cooked asset parsing requires a texture resolver to bind file-backed textures");
        }
        return texture_resolver(path, name);
    }
    throw std::runtime_error(std::format("Unknown cooked material parameter type: {}", type));
}

auto ShaderDescFromRecord(const CookedReader& reader, const ShaderRecord& record, const std::filesystem::path& asset_root_path) -> gfx::ShaderDesc {
    gfx::ShaderDesc desc{
        .name  = std::pmr::string(reader.GetString(record.name)),
        .type  = magic_enum::enum_cast<gfx::ShaderType>(reader.GetString(record.type)).value_or(gfx::ShaderType::Pixel),
        .entry = std::pmr::string(record.entry.length != 0 ? reader.GetString(record.entry) : "main"),
    };
    for (const auto& module : reader.ReadArray<ShaderModuleRecord>(record.modules)) desc.program.modules.push_back({
        .name        = std::pmr::string(reader.GetString(module.name)),
        .source_code = std::pmr::string(reader.GetString(module.source)),
        .path        = ResolveCookedPath(reader.GetString(module.path), asset_root_path),
    });
    for (const auto& type : reader.ReadArray<StringRef>(record.specialization_types))
        desc.specialization_types.emplace_back(reader.GetString(type));
    return desc;
}

auto PipelineDescFromRecord(const CookedReader& reader, const PipelineRecord& record, std::string_view fallback_name) -> gfx::RenderPipelineDesc {
    gfx::VertexLayout layout;
    for (const auto& attribute : reader.ReadArray<VertexInputRecord>(record.vertex_input_layout)) {
        const auto format = magic_enum::enum_cast<gfx::Format>(reader.GetString(attribute.format));
        if (!format || *format == gfx::Format::UNKNOWN || attribute.per_instance > 1)
            throw std::invalid_argument("Invalid cooked vertex input attribute");
        layout.push_back({.semantic = std::pmr::string(reader.GetString(attribute.semantic)), .format = *format, .binding = attribute.binding, .offset = attribute.offset, .stride = attribute.stride, .per_instance = attribute.per_instance != 0, .location = attribute.location});
    }
    gfx::RenderPipelineDesc desc{
        .name                = std::pmr::string(record.name.length != 0 ? reader.GetString(record.name) : fallback_name),
        .assembly_state      = {.primitive = magic_enum::enum_cast<gfx::PrimitiveTopology>(reader.GetString(record.primitive)).value_or(gfx::PrimitiveTopology::TriangleList)},
        .vertex_input_layout = std::move(layout),
        .rasterization_state = {
            .cull_mode               = magic_enum::enum_cast<gfx::CullMode>(reader.GetString(record.cull_mode)).value_or(gfx::CullMode::None),
            .front_counter_clockwise = record.front_counter_clockwise != 0,
        },
        .depth_stencil_state = {
            .depth_test_enable  = record.depth_test != 0,
            .depth_write_enable = record.depth_write != 0,
            .depth_compare_op   = magic_enum::enum_cast<gfx::CompareOp>(reader.GetString(record.depth_compare)).value_or(gfx::CompareOp::Less),
        },
        .render_format        = magic_enum::enum_cast<gfx::Format>(reader.GetString(record.render_format)).value_or(gfx::Format::R8G8B8A8_UNORM),
        .depth_stencil_format = magic_enum::enum_cast<gfx::Format>(reader.GetString(record.depth_stencil_format)).value_or(gfx::Format::D32_FLOAT),
    };
    desc.assembly_state.restart_enable                = record.restart_enable != 0;
    desc.rasterization_state.fill_mode                = magic_enum::enum_cast<gfx::FillMode>(reader.GetString(record.fill_mode)).value();
    desc.rasterization_state.depth_clamp_enable       = record.depth_clamp_enable != 0;
    desc.rasterization_state.depth_bias_enable        = record.depth_bias_enable != 0;
    desc.rasterization_state.depth_bias               = record.depth_bias;
    desc.rasterization_state.depth_bias_clamp         = record.depth_bias_clamp;
    desc.rasterization_state.depth_bias_slope_factor  = record.depth_bias_slope_factor;
    desc.depth_stencil_state.stencil_test_enable      = record.stencil_test_enable != 0;
    desc.depth_stencil_state.front.fail_op            = magic_enum::enum_cast<gfx::StencilOp>(reader.GetString(record.front_fail_op)).value();
    desc.depth_stencil_state.front.pass_op            = magic_enum::enum_cast<gfx::StencilOp>(reader.GetString(record.front_pass_op)).value();
    desc.depth_stencil_state.front.depth_fail_op      = magic_enum::enum_cast<gfx::StencilOp>(reader.GetString(record.front_depth_fail_op)).value();
    desc.depth_stencil_state.front.compare_op         = magic_enum::enum_cast<gfx::CompareOp>(reader.GetString(record.front_compare_op)).value();
    desc.depth_stencil_state.front.compare_mask       = record.front_compare_mask;
    desc.depth_stencil_state.front.write_mask         = record.front_write_mask;
    desc.depth_stencil_state.front.reference          = record.front_reference;
    desc.depth_stencil_state.back.fail_op             = magic_enum::enum_cast<gfx::StencilOp>(reader.GetString(record.back_fail_op)).value();
    desc.depth_stencil_state.back.pass_op             = magic_enum::enum_cast<gfx::StencilOp>(reader.GetString(record.back_pass_op)).value();
    desc.depth_stencil_state.back.depth_fail_op       = magic_enum::enum_cast<gfx::StencilOp>(reader.GetString(record.back_depth_fail_op)).value();
    desc.depth_stencil_state.back.compare_op          = magic_enum::enum_cast<gfx::CompareOp>(reader.GetString(record.back_compare_op)).value();
    desc.depth_stencil_state.back.compare_mask        = record.back_compare_mask;
    desc.depth_stencil_state.back.write_mask          = record.back_write_mask;
    desc.depth_stencil_state.back.reference           = record.back_reference;
    desc.depth_stencil_state.depth_bounds_test_enable = record.depth_bounds_test_enable != 0;
    desc.depth_stencil_state.depth_bounds             = record.depth_bounds;
    desc.blend_state.blend_enable                     = record.blend_enable != 0;
    desc.blend_state.src_color_blend_factor           = magic_enum::enum_cast<gfx::BlendFactor>(reader.GetString(record.src_color_blend_factor)).value();
    desc.blend_state.dst_color_blend_factor           = magic_enum::enum_cast<gfx::BlendFactor>(reader.GetString(record.dst_color_blend_factor)).value();
    desc.blend_state.color_blend_op                   = magic_enum::enum_cast<gfx::BlendOp>(reader.GetString(record.color_blend_op)).value();
    desc.blend_state.src_alpha_blend_factor           = magic_enum::enum_cast<gfx::BlendFactor>(reader.GetString(record.src_alpha_blend_factor)).value();
    desc.blend_state.dst_alpha_blend_factor           = magic_enum::enum_cast<gfx::BlendFactor>(reader.GetString(record.dst_alpha_blend_factor)).value();
    desc.blend_state.alpha_blend_op                   = magic_enum::enum_cast<gfx::BlendOp>(reader.GetString(record.alpha_blend_op)).value();
    desc.blend_state.blend_constants                  = record.blend_constants;
    desc.blend_state.color_write_mask                 = static_cast<gfx::ColorMask>(record.color_write_mask);
    desc.blend_state.logic_operation_enable           = record.logic_operation_enable != 0;
    desc.blend_state.logic_op                         = magic_enum::enum_cast<gfx::LogicOp>(reader.GetString(record.logic_op)).value();
    return desc;
}

auto PassFromRecord(const CookedReader& reader, const PassRecord& record, const std::filesystem::path& asset_root_path, const CookedTextureResolver& texture_resolver) -> MaterialPass {
    MaterialPass pass;
    pass.pass_contract = std::pmr::string(reader.GetString(record.contract));

    auto shaders = reader.ReadArray<ShaderRecord>(record.shaders) |
                   std::views::transform([&](const ShaderRecord& shader) {
                       return std::make_shared<Shader>(ShaderDescFromRecord(reader, shader, asset_root_path));
                   }) |
                   std::ranges::to<std::pmr::vector<std::shared_ptr<Shader>>>();
    if (record.has_pipeline != 0 && !shaders.empty()) {
        pass.pipeline = std::make_shared<RenderPipeline>(
            PipelineDescFromRecord(reader, record.pipeline, std::format("{}Pipeline", pass.pass_contract)),
            std::move(shaders));
    }

    pass.bindings = reader.ReadArray<ParameterRecord>(record.bindings) |
                    std::views::transform([&](const ParameterRecord& binding) {
                        return MaterialBinding{reader.GetString(binding.name), ParameterValueFromRecord(reader, binding, asset_root_path, texture_resolver)};
                    }) |
                    std::ranges::to<std::pmr::vector<MaterialBinding>>();
    return pass;
}

auto MaterialFromRecord(const CookedReader& reader, const MaterialRecord& record, const std::filesystem::path& asset_root_path, const CookedTextureResolver& texture_resolver) -> std::shared_ptr<Material> {
    auto parameters = reader.ReadArray<ParameterRecord>(record.parameters) |
                      std::views::transform([&](const ParameterRecord& parameter) {
                          return MaterialParameter{
                              .name  = std::pmr::string(reader.GetString(parameter.name)),
                              .value = ParameterValueFromRecord(reader, parameter, asset_root_path, texture_resolver),
                          };
                      }) |
                      std::ranges::to<MaterialParameters>();

    auto passes = reader.ReadArray<PassRecord>(record.passes) |
                  std::views::transform([&](const PassRecord& pass) { return PassFromRecord(reader, pass, asset_root_path, texture_resolver); }) |
                  std::ranges::to<std::pmr::vector<MaterialPass>>();

    return std::make_shared<Material>(std::move(parameters), std::move(passes), reader.GetString(record.name));
}

auto MeshFromRecord(const CookedReader& reader, const MeshRecord& record, std::span<const std::shared_ptr<Material>> materials) -> std::shared_ptr<Mesh> {
    const auto name = reader.GetString(record.name);
    VertexArray::Data                   data;
    std::unordered_set<VertexAttribute> seen;
    for (const auto attribute_record : reader.ReadArray<VertexAttributeRecord>(record.attributes)) {
        const auto attribute = magic_enum::enum_cast<VertexAttribute>(reader.GetString(attribute_record.attribute));
        if (!attribute || !seen.insert(*attribute).second) throw std::runtime_error("Invalid or duplicate vertex attribute");
        const auto bytes  = reader.GetBlob(attribute_record.data);
        const auto stride = get_vertex_attribute_size(*attribute);
        if (bytes.size() % stride || bytes.size() / stride != record.vertex_count)
            throw std::runtime_error("Cooked vertex attribute count mismatch");
        data[*attribute] = core::Buffer(bytes);
    }
    auto       vertices = std::make_shared<VertexArray>(record.vertex_count, std::move(data), name);
    const auto type     = magic_enum::enum_cast<IndexType>(reader.GetString(record.index_type));
    if (!type) throw std::runtime_error("Invalid cooked index type");
    auto indices = std::make_shared<IndexArray>(*type, core::Buffer(reader.GetBlob(record.indices)), name);
    if (indices->Size() != record.index_count) throw std::runtime_error("Cooked index count mismatch");
    std::pmr::vector<Mesh::SubMesh> sub_meshes;
    for (const auto sub_mesh : reader.ReadArray<SubMeshRecord>(record.sub_meshes)) {
        if (sub_mesh.material_index != kCookedInvalidIndex && sub_mesh.material_index >= materials.size())
            throw std::runtime_error("Invalid submesh material reference");
        sub_meshes.push_back({.index_count = sub_mesh.index_count, .index_offset = sub_mesh.index_offset, .vertex_offset = sub_mesh.vertex_offset, .material = sub_mesh.material_index == kCookedInvalidIndex ? nullptr : materials[sub_mesh.material_index]});
    }
    return std::make_shared<Mesh>(std::move(vertices), std::move(indices), std::move(sub_meshes), name);
}

auto CameraParametersFromRecord(const CameraRecord& record) -> CameraComponent {
    return {
        .aspect         = record.aspect,
        .near_clip      = record.near_clip,
        .far_clip       = record.far_clip,
        .horizontal_fov = record.horizontal_fov,
    };
}

auto LightParametersFromRecord(const CookedReader& reader, const LightRecord& record) -> LightComponent {
    return {
        .type             = magic_enum::enum_cast<LightComponent::Type>(reader.GetString(record.type)).value_or(LightComponent::Type::Spot),
        .intensity        = record.intensity,
        .color            = record.color,
        .inner_cone_angle = record.inner_cone_angle,
        .outer_cone_angle = record.outer_cone_angle,
    };
}

auto ParseCookedMaterial(std::span<const std::byte> data, const std::filesystem::path& asset_root_path, const CookedTextureResolver& texture_resolver) -> std::shared_ptr<Material> {
    const CookedReader reader(data, CookedAssetType::Material);
    return MaterialFromRecord(reader, reader.Read<MaterialRecord>(reader.GetHeader().root_offset), asset_root_path, texture_resolver);
}

template <typename T>
auto CurveFromRecord(const CookedReader& reader, const CurveRecord& record) -> AnimationCurve<T> {
    AnimationCurve<T> result;
    switch (record.interpolation) {
        case CurveInterpolation::Step:
            result.interpolation = AnimationInterpolation::Step;
            break;
        case CurveInterpolation::Linear:
            result.interpolation = AnimationInterpolation::Linear;
            break;
        default:
            throw std::runtime_error("Unknown cooked curve interpolation");
    }
    const auto values = reader.GetBlob(record.values);
    if (!record.key_count || values.size() / sizeof(T) != record.key_count || values.size() % sizeof(T))
        throw std::runtime_error("Invalid cooked curve values");
    const auto times = reader.GetBlob(record.times);
    if (record.time_encoding == CurveTimeEncoding::Explicit) {
        if (times.size() / sizeof(double) != record.key_count || times.size() % sizeof(double))
            throw std::runtime_error("Invalid cooked curve times");
    } else if (record.time_encoding == CurveTimeEncoding::Uniform) {
        if (!times.empty() || !std::isfinite(record.start) || !std::isfinite(record.step) ||
            (record.key_count > 1 ? record.step <= 0 : record.step != 0))
            throw std::runtime_error("Invalid uniform curve times");
    } else
        throw std::runtime_error("Unknown cooked curve time encoding");
    result.keys.resize(record.key_count);
    for (std::size_t i = 0; i < result.keys.size(); ++i) {
        auto& key = result.keys[i];
        std::memcpy(&key.value, values.data() + i * sizeof(T), sizeof(T));
        if (record.time_encoding == CurveTimeEncoding::Explicit)
            std::memcpy(&key.time, times.data() + i * sizeof(double), sizeof(double));
        else
            key.time = std::fma(double(i), record.step, record.start);
    }
    return result;
}

auto ClipFromRecord(const CookedReader& reader, const AnimationClipRecord& record) -> std::shared_ptr<AnimationClip> {
    std::vector<AnimationTrack> tracks;
    for (const auto track : reader.ReadArray<AnimationTrackRecord>(record.tracks)) {
        AnimationTrack      decoded{.target = track.target};
        std::array<bool, 3> present{};
        for (const auto curve : reader.ReadArray<CurveRecord>(track.curves)) {
            const auto property = static_cast<unsigned>(curve.property);
            if (property >= present.size() || std::exchange(present[property], true)) throw std::runtime_error("Invalid or duplicate animation channel");
            switch (curve.property) {
                case CurveProperty::Translation:
                    decoded.positions = CurveFromRecord<vec3f>(reader, curve);
                    break;
                case CurveProperty::Rotation:
                    decoded.rotations = CurveFromRecord<quatf>(reader, curve);
                    break;
                case CurveProperty::Scale:
                    decoded.scales = CurveFromRecord<vec3f>(reader, curve);
                    break;
            }
        }
        tracks.emplace_back(std::move(decoded));
    }
    // Constructor checks duration, finite values, strictly increasing key times,
    // quaternion validity and duplicate target IDs before the scene is published.
    return std::make_shared<AnimationClip>(record.duration, std::move(tracks), reader.GetString(record.name));
}

auto ParseCookedScene(std::span<const std::byte> data, const std::filesystem::path& asset_root_path, core::JobSystem& job_system, const CookedTextureResolver& texture_resolver) -> std::shared_ptr<Scene> {
    const CookedReader reader(data, CookedAssetType::Scene);
    const auto         root  = reader.Read<SceneRecord>(reader.GetHeader().root_offset);
    const auto         nodes = reader.ReadArray<NodeRecord>(root.nodes) | std::ranges::to<std::vector>();
    if (nodes.size() >= kCookedInvalidIndex) throw std::runtime_error("Invalid cooked scene node count");
    std::vector<unsigned char> marks(nodes.size());
    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        for (float v : node.translation.data)
            if (!std::isfinite(v)) throw std::runtime_error("Non-finite node translation");
        for (float v : node.scale.data)
            if (!std::isfinite(v)) throw std::runtime_error("Non-finite node scale");
        const float length = node.rotation.norm();
        if (!std::isfinite(length) || std::abs(length - 1) > 1e-3f) throw std::runtime_error("Invalid node quaternion");
        auto                       cursor = i;
        std::vector<std::uint32_t> path;
        while (cursor < nodes.size() && !marks[cursor]) {
            marks[cursor] = 1;
            path.emplace_back(cursor);
            cursor = nodes[cursor].parent;
        }
        if (cursor != kCookedInvalidIndex && (cursor >= nodes.size() || marks[cursor] == 1))
            throw std::runtime_error("Invalid or cyclic node hierarchy");
        for (auto index : path) marks[index] = 2;
    }
    const auto                         materials = reader.ReadArray<MaterialRecord>(root.materials) |
                                                   std::views::transform([&](const auto& record) { return MaterialFromRecord(reader, record, asset_root_path, texture_resolver); }) |
                                                   std::ranges::to<std::vector>();
    const auto                         meshes    = reader.ReadArray<MeshRecord>(root.meshes) |
                                                   std::views::transform([&](const auto& record) { return MeshFromRecord(reader, record, materials); }) |
                                                   std::ranges::to<std::vector>();
    std::vector<std::shared_ptr<Skin>> skins;
    for (const auto record : reader.ReadArray<SkinRecord>(root.skins)) {
        const auto bytes = reader.GetBlob(record.inverse_bind_matrices);
        if (bytes.empty() || bytes.size() % sizeof(AffineMatrixRecord)) throw std::runtime_error("Invalid affine matrix array");
        std::vector<mat4f> matrices(bytes.size() / sizeof(AffineMatrixRecord), mat4f::identity());
        for (std::size_t i = 0; i < matrices.size(); ++i) {
            AffineMatrixRecord packed;
            std::memcpy(&packed, bytes.data() + i * sizeof(packed), sizeof(packed));
            for (unsigned row = 0; row < 3; ++row)
                for (unsigned column = 0; column < 4; ++column)
                    matrices[i][row][column] = packed.values[row * 4 + column];
        }
        skins.emplace_back(std::make_shared<Skin>(std::move(matrices), reader.GetString(record.name)));
    }
    const auto               clips = reader.ReadArray<AnimationClipRecord>(root.clips) |
                                     std::views::transform([&](const auto& record) { return ClipFromRecord(reader, record); }) |
                                     std::ranges::to<std::vector>();
    auto                     scene = std::make_shared<Scene>(reader.GetString(root.name));
    std::vector<ecs::Entity> entities(nodes.size());
    // Allocate all identities first; binding records may refer forward or backward.
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto name = reader.GetString(nodes[i].name);
        entities[i]     = scene->GetWorld().GetEntityManager().Create(
            Transform(nodes[i].translation, nodes[i].rotation, nodes[i].scale), RelationShip{}, MetaInfo(name));
    }
    const auto entity_at = [&](std::uint32_t index) {
        if (index >= entities.size()) throw std::runtime_error("Cooked node reference out of range");
        return entities[index];
    };
    for (std::size_t i = 0; i < nodes.size(); ++i)
        entities[i].Get<RelationShip>().parent = nodes[i].parent == kCookedInvalidIndex ? ecs::Entity{} : entity_at(nodes[i].parent);
    for (const auto instance : reader.ReadArray<MeshInstanceRecord>(root.instances)) {
        auto entity = entity_at(instance.node);
        if (instance.mesh_index >= meshes.size() || entity.Has<MeshComponent>())
            throw std::runtime_error("Invalid or duplicate mesh component");
        const auto& mesh = meshes[instance.mesh_index];
        if (instance.material_overrides.count > mesh->sub_meshes.size()) throw std::runtime_error("Too many material overrides");
        std::vector<std::shared_ptr<Material>> overrides;
        for (auto index : reader.ReadArray<std::uint32_t>(instance.material_overrides)) {
            if (index != kCookedInvalidIndex && index >= materials.size()) throw std::runtime_error("Invalid material override");
            overrides.push_back(index == kCookedInvalidIndex ? nullptr : materials[index]);
        }
        entity.Emplace<MeshComponent>(mesh, std::move(overrides));
    }
    for (const auto camera : reader.ReadArray<CameraRecord>(root.cameras)) {
        auto entity = entity_at(camera.node);
        if (entity.Has<CameraComponent>()) throw std::runtime_error("Duplicate camera component");
        entity.Emplace<CameraComponent>(CameraParametersFromRecord(camera));
    }
    for (const auto light : reader.ReadArray<LightRecord>(root.lights)) {
        auto entity = entity_at(light.node);
        if (entity.Has<LightComponent>()) throw std::runtime_error("Duplicate light component");
        entity.Emplace<LightComponent>(LightParametersFromRecord(reader, light));
    }
    for (const auto record : reader.ReadArray<AnimationPlayerRecord>(root.players)) {
        auto entity = entity_at(record.node);
        if (entity.Has<AnimationPlayer>() || (record.clip != kCookedInvalidIndex && record.clip >= clips.size()) ||
            record.playing > 1 || record.looping > 1 || !std::isfinite(record.time) || !std::isfinite(record.speed))
            throw std::runtime_error("Invalid animation player");
        entity.Emplace<AnimationPlayer>(record.clip == kCookedInvalidIndex ? nullptr : clips[record.clip], record.time, record.speed, bool(record.looping), bool(record.playing));
    }
    for (const auto record : reader.ReadArray<AnimationTargetRecord>(root.targets)) {
        auto entity = entity_at(record.node);
        auto player = entity_at(record.player_node);
        if (entity.Has<AnimationTarget>() || !player.Has<AnimationPlayer>()) throw std::runtime_error("Invalid animation target binding");
        entity.Emplace<AnimationTarget>(player, record.target);
    }
    for (const auto record : reader.ReadArray<SkinnedMeshRecord>(root.skinned_meshes)) {
        auto entity = entity_at(record.node);
        if (entity.Has<SkinnedMesh>() || !entity.Has<MeshComponent>() || record.skin >= skins.size() ||
            record.joints.count != skins[record.skin]->GetInverseBindMatrices().size()) throw std::runtime_error("Invalid skin binding");
        std::vector<ecs::Entity> joints;
        for (auto node : reader.ReadArray<std::uint32_t>(record.joints)) joints.emplace_back(entity_at(node));
        entity.Emplace<SkinnedMesh>(skins[record.skin], std::move(joints));
    }
    // Evaluates the serialized time, without advancing it, and rebuilds caches.
    scene->GetWorld().Update(job_system, 0.0);
    return scene;
}

}  // namespace hitagi::asset
