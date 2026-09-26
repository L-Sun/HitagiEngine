module;

// Implementation partition (not exported from the module interface): the cooked
// parsers stay internal to hitagi::asset, while sibling partitions such as
// :manager can `import :cooked_binary;` to see these declarations.
module asset:cooked_binary;
import std;
import core;
import gfx;
import math;
import magic_enum;
import :cooked_format;
import :shader;
import :pipeline;
import :texture;
import :material;
import :mesh;
import :camera;
import :light;
import :scene;

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
    const auto path = ResolveCookedPath(reader.GetString(record.path), asset_root_path);
    return {
        .name        = std::pmr::string(record.name.length != 0 ? std::string(reader.GetString(record.name)) : path.string()),
        .type        = magic_enum::enum_cast<gfx::ShaderType>(reader.GetString(record.type)).value_or(gfx::ShaderType::Pixel),
        .entry       = std::pmr::string(record.entry.length != 0 ? reader.GetString(record.entry) : "main"),
        .source_code = std::pmr::string(reader.GetString(record.source)),
        .path        = path,
    };
}

auto PipelineDescFromRecord(const CookedReader& reader, const PipelineRecord& record, std::string_view fallback_name) -> gfx::RenderPipelineDesc {
    return {
        .name                = std::pmr::string(record.name.length != 0 ? reader.GetString(record.name) : fallback_name),
        .assembly_state      = {.primitive = magic_enum::enum_cast<gfx::PrimitiveTopology>(reader.GetString(record.primitive)).value_or(gfx::PrimitiveTopology::TriangleList)},
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
}

auto PassFromRecord(const CookedReader& reader, const PassRecord& record, const std::filesystem::path& asset_root_path) -> MaterialPass {
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

    pass.bindings = reader.ReadArray<StringRef>(record.bindings) |
                    std::views::transform([&](StringRef binding) { return std::pmr::string(reader.GetString(binding)); }) |
                    std::ranges::to<std::pmr::vector<std::pmr::string>>();
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
                  std::views::transform([&](const PassRecord& pass) { return PassFromRecord(reader, pass, asset_root_path); }) |
                  std::ranges::to<std::pmr::vector<MaterialPass>>();

    return std::make_shared<Material>(std::move(parameters), std::move(passes), reader.GetString(record.name));
}

void CopyVertexAttribute(VertexArray& vertices, VertexAttribute attribute, std::span<const std::byte> data) {
    magic_enum::enum_for_each<VertexAttribute>([&](auto attr) {
        constexpr VertexAttribute Attr = attr;
        if (attribute != Attr) return;
        vertices.Modify<Attr>([&](std::span<VertexDataType<Attr>> destination) {
            if (data.size() != destination.size_bytes()) {
                throw std::runtime_error("Cooked vertex attribute data size mismatch.");
            }
            std::memcpy(destination.data(), data.data(), data.size());
        });
    });
}

auto MeshFromRecord(const CookedReader& reader, const MeshRecord& record, std::span<const std::shared_ptr<Material>> materials) -> std::shared_ptr<Mesh> {
    const auto name = reader.GetString(record.name);

    auto vertices = std::make_shared<VertexArray>(record.vertex_count, name);
    for (const auto attribute_record : reader.ReadArray<VertexAttributeRecord>(record.attributes)) {
        if (const auto attribute = magic_enum::enum_cast<VertexAttribute>(reader.GetString(attribute_record.attribute))) {
            CopyVertexAttribute(*vertices, *attribute, reader.GetBlob(attribute_record.data));
        }
    }

    const auto index_type = magic_enum::enum_cast<IndexType>(reader.GetString(record.index_type)).value_or(IndexType::UINT16);
    auto       indices    = std::make_shared<IndexArray>(record.index_count, index_type, name);
    const auto index_data = reader.GetBlob(record.indices);
    const auto copy_indices = [&](auto destination) {
        if (index_data.size() != destination.size_bytes()) {
            throw std::runtime_error("Cooked index data size mismatch.");
        }
        std::memcpy(destination.data(), index_data.data(), index_data.size());
    };
    if (index_type == IndexType::UINT16) {
        indices->Modify<IndexType::UINT16>(copy_indices);
    } else {
        indices->Modify<IndexType::UINT32>(copy_indices);
    }

    auto mesh = std::make_shared<Mesh>(std::move(vertices), std::move(indices), name);
    for (const auto sub_mesh : reader.ReadArray<SubMeshRecord>(record.sub_meshes)) {
        mesh->AddSubMesh({
            .index_count   = sub_mesh.index_count,
            .index_offset  = sub_mesh.index_offset,
            .vertex_offset = sub_mesh.vertex_offset,
            .material      = sub_mesh.material_index < materials.size() ? materials[sub_mesh.material_index] : nullptr,
        });
    }
    mesh->aabb = {
        .min_point = record.aabb_min,
        .max_point = record.aabb_max,
    };
    return mesh;
}

auto CameraParametersFromRecord(const CameraRecord& record) -> Camera::Parameters {
    return {
        .aspect         = record.aspect,
        .near_clip      = record.near_clip,
        .far_clip       = record.far_clip,
        .horizontal_fov = record.horizontal_fov,
        .eye            = record.eye,
        .look_dir       = record.look_dir,
        .up             = record.up,
    };
}

auto LightParametersFromRecord(const CookedReader& reader, const LightRecord& record) -> Light::Parameters {
    return {
        .type             = magic_enum::enum_cast<Light::Type>(reader.GetString(record.type)).value_or(Light::Type::Spot),
        .intensity        = record.intensity,
        .color            = record.color,
        .position         = record.position,
        .direction        = record.direction,
        .up               = record.up,
        .inner_cone_angle = record.inner_cone_angle,
        .outer_cone_angle = record.outer_cone_angle,
    };
}

auto ParseCookedMaterial(std::span<const std::byte> data, const std::filesystem::path& asset_root_path, const CookedTextureResolver& texture_resolver) -> std::shared_ptr<Material> {
    const CookedReader reader(data, CookedAssetType::Material);
    return MaterialFromRecord(reader, reader.Read<MaterialRecord>(reader.GetHeader().root_offset), asset_root_path, texture_resolver);
}

auto ParseCookedScene(std::span<const std::byte> data, const std::filesystem::path& asset_root_path, const CookedTextureResolver& texture_resolver) -> std::shared_ptr<Scene> {
    const CookedReader reader(data, CookedAssetType::Scene);
    const auto         root = reader.Read<SceneRecord>(reader.GetHeader().root_offset);

    const auto materials = reader.ReadArray<MaterialRecord>(root.materials) |
                           std::views::transform([&](const MaterialRecord& material) { return MaterialFromRecord(reader, material, asset_root_path, texture_resolver); }) |
                           std::ranges::to<std::pmr::vector<std::shared_ptr<Material>>>();

    const auto meshes = reader.ReadArray<MeshRecord>(root.meshes) |
                        std::views::transform([&](const MeshRecord& mesh) { return MeshFromRecord(reader, mesh, materials); }) |
                        std::ranges::to<std::pmr::vector<std::shared_ptr<Mesh>>>();

    auto scene = std::make_shared<Scene>(reader.GetString(root.name));
    for (const auto instance : reader.ReadArray<MeshInstanceRecord>(root.instances)) {
        if (instance.mesh_index >= meshes.size()) continue;
        scene->CreateMeshEntity(
            meshes[instance.mesh_index],
            instance.transform,
            scene->GetRootEntity(),
            reader.GetString(instance.name));
    }

    for (const auto camera : reader.ReadArray<CameraRecord>(root.cameras)) {
        const auto name = reader.GetString(camera.name);
        scene->CreateCameraEntity(
            std::make_shared<Camera>(CameraParametersFromRecord(camera), name),
            camera.transform,
            scene->GetRootEntity(),
            name);
    }

    for (const auto light : reader.ReadArray<LightRecord>(root.lights)) {
        const auto name = reader.GetString(light.name);
        scene->CreateLightEntity(
            std::make_shared<Light>(LightParametersFromRecord(reader, light), name),
            light.transform,
            scene->GetRootEntity(),
            name);
    }

    scene->Update();
    return scene;
}

}  // namespace hitagi::asset
