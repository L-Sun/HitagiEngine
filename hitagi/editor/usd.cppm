export module editor:usd;
import interop.usd;
import interop.spdlog;

import engine;
import std;
import :cook;

using namespace hitagi::asset;
using namespace hitagi::math;

export namespace hitagi {

enum struct EditorSceneFormat : std::uint8_t {
    Unknown,
    USD,
    USDA,
    USDC,
    USDZ,
    Cooked,
};

auto GetEditorSceneFormat(std::string_view extension) noexcept -> EditorSceneFormat;
auto IsEditorScenePath(const std::filesystem::path& path) noexcept -> bool;

class UsdSceneImporter final {
public:
    using MaterialGetter    = std::function<std::shared_ptr<asset::Material>(std::string_view)>;
    using MaterialProcessor = std::function<void(EditorMaterial&)>;

    explicit UsdSceneImporter(
        MaterialGetter                  material_getter    = {},
        std::shared_ptr<spdlog::logger> logger             = nullptr,
        MaterialProcessor               material_processor = {},
        EditorCookContext*              cook_context       = nullptr);

    auto Import(
        const std::filesystem::path& path,
        const std::filesystem::path& resource_base_path = {}) -> std::shared_ptr<asset::Scene>;

private:
    MaterialGetter                  m_MaterialGetter;
    std::shared_ptr<spdlog::logger> m_Logger;
    MaterialProcessor               m_MaterialProcessor;
    EditorCookContext*              m_CookContext = nullptr;
};

// Cooked scenes are loaded (and registered) through `asset_manager`; USD scenes
// are converted in place and are NOT registered automatically (callers decide).
auto ImportEditorScene(
    asset::AssetManager&                asset_manager,
    const std::filesystem::path&        path,
    const std::filesystem::path&        resource_base_path = {},
    UsdSceneImporter::MaterialGetter    material_getter    = {},
    std::shared_ptr<spdlog::logger>     logger             = nullptr,
    UsdSceneImporter::MaterialProcessor material_processor = {},
    EditorCookContext*                  cook_context       = nullptr) -> std::shared_ptr<asset::Scene>;

}  // namespace hitagi

namespace hitagi {
namespace {

struct ExpandedMeshTopology {
    std::vector<std::uint32_t> point_indices;
    std::vector<std::size_t>   face_vertex_indices;
    std::vector<std::size_t>   face_indices;
};

using TextureCache  = std::pmr::unordered_map<std::string, std::shared_ptr<Texture>>;
using MaterialCache = std::pmr::unordered_map<std::string, EditorMaterial>;

auto ToMat4f(const pxr::GfMatrix4d& matrix) noexcept -> mat4f {
    mat4f result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            result[row][col] = static_cast<float>(matrix[row][col]);
        }
    }
    return result;
}

auto ToVec2f(const pxr::GfVec2f& value) noexcept -> vec2f {
    return {value[0], value[1]};
}

auto ToVec3f(const pxr::GfVec3f& value) noexcept -> vec3f {
    return {value[0], value[1], value[2]};
}

auto ToVec4f(const pxr::GfVec4f& value) noexcept -> vec4f {
    return {value[0], value[1], value[2], value[3]};
}

auto ToColor(const pxr::GfVec3f& value, float alpha = 1.0f) noexcept -> Color {
    return {value[0], value[1], value[2], alpha};
}

auto GetLocalTransform(const pxr::UsdPrim& prim) -> mat4f {
    pxr::GfMatrix4d transform(1.0);
    bool            resets_xform_stack = false;

    const pxr::UsdGeomXformable xformable(prim);
    if (xformable) {
        xformable.GetLocalTransformation(&transform, &resets_xform_stack, pxr::UsdTimeCode::Default());
    }

    return ToMat4f(transform);
}

auto BuildExpandedTopology(
    const pxr::VtArray<int>& face_vertex_counts,
    const pxr::VtArray<int>& face_vertex_indices) -> ExpandedMeshTopology {
    ExpandedMeshTopology topology;

    std::size_t face_vertex_offset = 0;
    for (std::size_t face_index = 0; face_index < face_vertex_counts.size(); ++face_index) {
        const int face_vertex_count = face_vertex_counts[face_index];
        if (face_vertex_count < 3) {
            face_vertex_offset += static_cast<std::size_t>(std::max(face_vertex_count, 0));
            continue;
        }
        if (face_vertex_offset + static_cast<std::size_t>(face_vertex_count) > face_vertex_indices.size()) break;

        for (int vertex = 1; vertex + 1 < face_vertex_count; ++vertex) {
            const std::array<std::size_t, 3> fan_indices{
                face_vertex_offset,
                face_vertex_offset + static_cast<std::size_t>(vertex),
                face_vertex_offset + static_cast<std::size_t>(vertex + 1),
            };

            for (const auto face_vertex_index : fan_indices) {
                const int point_index = face_vertex_indices[face_vertex_index];
                if (point_index < 0) continue;

                topology.point_indices.emplace_back(static_cast<std::uint32_t>(point_index));
                topology.face_vertex_indices.emplace_back(face_vertex_index);
                topology.face_indices.emplace_back(face_index);
            }
        }

        face_vertex_offset += static_cast<std::size_t>(face_vertex_count);
    }

    return topology;
}

auto SelectValueIndex(
    const pxr::TfToken&         interpolation,
    const ExpandedMeshTopology& topology,
    std::size_t                 expanded_index,
    std::size_t                 value_count) -> std::size_t {
    if (value_count == 0) return 0;
    if (value_count == topology.point_indices.size()) return expanded_index;

    std::size_t index = topology.point_indices[expanded_index];
    if (interpolation == pxr::UsdGeomTokens->constant) {
        index = 0;
    } else if (interpolation == pxr::UsdGeomTokens->uniform) {
        index = topology.face_indices[expanded_index];
    } else if (interpolation == pxr::UsdGeomTokens->faceVarying) {
        index = topology.face_vertex_indices[expanded_index];
    }

    return std::min(index, value_count - 1);
}

auto ComputeFaceNormals(const std::vector<vec3f>& positions) -> std::vector<vec3f> {
    std::vector<vec3f> normals(positions.size(), vec3f{0.0f, 0.0f, 1.0f});

    for (std::size_t i = 0; i + 2 < positions.size(); i += 3) {
        const auto edge_0 = positions[i + 1] - positions[i];
        const auto edge_1 = positions[i + 2] - positions[i];
        auto       normal = cross(edge_0, edge_1);
        normal            = normal.norm() > std::numeric_limits<float>::epsilon() ? normalize(normal) : vec3f{0.0f, 0.0f, 1.0f};
        normals[i] = normals[i + 1] = normals[i + 2] = normal;
    }

    return normals;
}

auto DecodeUsdTexture(
    const pxr::SdfAssetPath&               asset_path,
    TextureCache&                          texture_cache,
    const std::shared_ptr<spdlog::logger>& logger) -> std::shared_ptr<Texture> {
    const auto& authored_path = asset_path.GetAssetPath();
    const auto& resolved_path = asset_path.GetResolvedPath();
    const auto  cache_key     = !resolved_path.empty() ? resolved_path : authored_path;
    if (cache_key.empty()) return nullptr;

    if (const auto iter = texture_cache.find(cache_key); iter != texture_cache.end()) return iter->second;

    auto& resolver = pxr::ArGetResolver();
    auto  resolved = !resolved_path.empty() ? pxr::ArResolvedPath(resolved_path) : resolver.Resolve(authored_path);
    if (!resolved) {
        if (logger) logger->warn("Failed to resolve USD texture asset: {}", authored_path);
        texture_cache.emplace(cache_key, nullptr);
        return nullptr;
    }

    const auto codec = create_image_codec_for(std::filesystem::path(authored_path).extension());
    if (!codec) {
        if (logger) logger->warn("Unsupported USD texture format: {}", authored_path);
        texture_cache.emplace(cache_key, nullptr);
        return nullptr;
    }

    auto asset = resolver.OpenAsset(resolved);
    if (!asset) {
        if (logger) logger->warn("Failed to open USD texture asset: {}", resolved.GetPathString());
        texture_cache.emplace(cache_key, nullptr);
        return nullptr;
    }

    const auto   size = asset->GetSize();
    core::Buffer buffer(size);
    if (asset->Read(buffer.GetData(), size, 0) != size) {
        if (logger) logger->warn("Failed to read USD texture asset: {}", resolved.GetPathString());
        texture_cache.emplace(cache_key, nullptr);
        return nullptr;
    }

    auto texture = codec->Decode(buffer);
    if (texture) {
        const auto& resolved_path_text = resolved.GetPathString();
        texture->SetName(authored_path.empty() ? resolved_path_text : authored_path);
        texture->SetPath(resolved_path_text);
    }
    texture_cache.emplace(cache_key, texture);
    return texture;
}

auto GetConnectedShader(
    const pxr::UsdShadeShader& shader,
    const pxr::TfToken&        input_name) -> std::optional<pxr::UsdShadeShader> {
    const auto input = shader.GetInput(input_name);
    if (!input) return std::nullopt;

    pxr::UsdShadeConnectableAPI source;
    pxr::TfToken                source_name;
    pxr::UsdShadeAttributeType  source_type = pxr::UsdShadeAttributeType::Invalid;
    if (!input.GetConnectedSource(&source, &source_name, &source_type)) return std::nullopt;

    auto source_shader = pxr::UsdShadeShader(source.GetPrim());
    if (!source_shader) return std::nullopt;
    return source_shader;
}

bool InputHasConnection(const pxr::UsdShadeShader& shader, const pxr::TfToken& input_name) {
    const auto input = shader.GetInput(input_name);
    if (!input) return false;

    pxr::UsdShadeConnectableAPI source;
    pxr::TfToken                source_name;
    pxr::UsdShadeAttributeType  source_type = pxr::UsdShadeAttributeType::Invalid;
    return input.GetConnectedSource(&source, &source_name, &source_type);
}

auto LoadConnectedTexture(
    const pxr::UsdShadeShader&             shader,
    const pxr::TfToken&                    input_name,
    TextureCache&                          texture_cache,
    const std::shared_ptr<spdlog::logger>& logger) -> std::shared_ptr<Texture> {
    const auto connected_shader = GetConnectedShader(shader, input_name);
    if (!connected_shader) return nullptr;

    pxr::TfToken shader_id;
    if (!connected_shader->GetIdAttr().Get(&shader_id) || shader_id != pxr::TfToken("UsdUVTexture")) return nullptr;

    const auto file_input = connected_shader->GetInput(pxr::TfToken("file"));
    if (!file_input) return nullptr;

    pxr::SdfAssetPath texture_path;
    if (!file_input.Get(&texture_path, pxr::UsdTimeCode::Default())) return nullptr;

    return DecodeUsdTexture(texture_path, texture_cache, logger);
}

void AppendTextureSource(MaterialSourceInfo& source_info, const std::shared_ptr<Texture>& texture) {
    if (texture && !texture->GetPath().empty()) source_info.source_texture_paths.emplace_back(texture->GetPath());
}

void AppendUnsupportedConnection(
    MaterialSourceInfo&        source_info,
    const pxr::UsdShadeShader& shader,
    const pxr::TfToken&        input_name) {
    source_info.unsupported_inputs.emplace_back(input_name.GetString());
    if (const auto connected_shader = GetConnectedShader(shader, input_name)) {
        if (pxr::TfToken shader_id; connected_shader->GetIdAttr().Get(&shader_id)) {
            source_info.unsupported_nodes.emplace_back(shader_id.GetString());
        }
    }
}

auto ReadShaderInputValue(const pxr::UsdShadeInput& input) -> std::optional<MaterialParameterValue> {
    if (float value; input.Get(&value, pxr::UsdTimeCode::Default())) return value;
    if (std::int32_t value; input.Get(&value, pxr::UsdTimeCode::Default())) return value;
    if (std::uint32_t value; input.Get(&value, pxr::UsdTimeCode::Default())) return value;
    if (bool value; input.Get(&value, pxr::UsdTimeCode::Default())) return static_cast<std::uint32_t>(value ? 1u : 0u);
    if (pxr::GfVec2f value; input.Get(&value, pxr::UsdTimeCode::Default())) return ToVec2f(value);
    if (pxr::GfVec3f value; input.Get(&value, pxr::UsdTimeCode::Default())) return ToVec3f(value);
    if (pxr::GfVec4f value; input.Get(&value, pxr::UsdTimeCode::Default())) return ToVec4f(value);
    if (pxr::GfMatrix4d value; input.Get(&value, pxr::UsdTimeCode::Default())) return ToMat4f(value);
    return std::nullopt;
}

auto BuildMaterialFromUsd(
    const pxr::UsdShadeMaterial&           usd_material,
    const std::filesystem::path&           source_asset,
    TextureCache&                          texture_cache,
    const std::shared_ptr<spdlog::logger>& logger) -> EditorMaterial {
    if (!usd_material) return {};

    const auto shader = usd_material.ComputeSurfaceSource(pxr::TfTokenVector{pxr::UsdShadeTokens->universalRenderContext});
    if (!shader) return {};

    pxr::TfToken shader_id;
    if (!shader.GetIdAttr().Get(&shader_id)) return {};

    MaterialSourceInfo source_info{
        .type                 = MaterialSourceType::Imported,
        .source_asset         = source_asset,
        .source_material_path = usd_material.GetPath().GetString(),
        .source_shader_id     = shader_id.GetString(),
    };
    MaterialParameters parameters;

    for (const auto& input : shader.GetInputs()) {
        const auto input_name = input.GetBaseName().GetString();
        if (InputHasConnection(shader, input.GetBaseName())) {
            if (auto texture = LoadConnectedTexture(shader, input.GetBaseName(), texture_cache, logger)) {
                AppendTextureSource(source_info, texture);
                parameters.emplace_back(MaterialParameter{
                    .name  = std::pmr::string(input_name),
                    .value = std::move(texture),
                });
            } else {
                AppendUnsupportedConnection(source_info, shader, input.GetBaseName());
            }
            continue;
        }

        if (auto value = ReadShaderInputValue(input)) {
            parameters.emplace_back(MaterialParameter{
                .name  = std::pmr::string(input_name),
                .value = std::move(*value),
            });
        } else {
            source_info.unsupported_inputs.emplace_back(input_name);
        }
    }

    return EditorMaterial{
        .material = std::make_shared<Material>(
            std::move(parameters),
            std::pmr::vector<MaterialPass>{},
            usd_material.GetPath().GetName()),
        .source_info = std::move(source_info),
    };
}

auto BuildMesh(
    const pxr::UsdGeomMesh&                    usd_mesh,
    MaterialCache&                             material_cache,
    TextureCache&                              texture_cache,
    const std::filesystem::path&               source_asset,
    const std::shared_ptr<spdlog::logger>&     logger,
    const UsdSceneImporter::MaterialProcessor& material_processor,
    EditorCookContext*                         cook_context) -> std::shared_ptr<Mesh> {
    pxr::VtArray<pxr::GfVec3f> points;
    pxr::VtArray<int>          face_vertex_counts;
    pxr::VtArray<int>          face_vertex_indices;

    if (!usd_mesh.GetPointsAttr().Get(&points, pxr::UsdTimeCode::Default()) ||
        !usd_mesh.GetFaceVertexCountsAttr().Get(&face_vertex_counts, pxr::UsdTimeCode::Default()) ||
        !usd_mesh.GetFaceVertexIndicesAttr().Get(&face_vertex_indices, pxr::UsdTimeCode::Default())) {
        return nullptr;
    }

    auto topology = BuildExpandedTopology(face_vertex_counts, face_vertex_indices);
    if (topology.point_indices.empty()) return nullptr;
    if (topology.point_indices.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("USD mesh is too large for uint32 index buffers");
    }

    const auto mesh_name    = usd_mesh.GetPrim().GetName().GetString();
    auto       vertices     = std::make_shared<VertexArray>(topology.point_indices.size(), std::format("{}-vertices", mesh_name));
    auto       indices      = std::make_shared<IndexArray>(topology.point_indices.size(), IndexType::UINT32, std::format("{}-indices", mesh_name));
    auto       position_cpu = std::vector<vec3f>(topology.point_indices.size());

    vertices->Modify<VertexAttribute::Position>([&](auto positions) {
        for (std::size_t i = 0; i < topology.point_indices.size(); ++i) {
            const auto point_index = std::min<std::size_t>(topology.point_indices[i], points.size() - 1);
            positions[i]           = ToVec3f(points[point_index]);
            position_cpu[i]        = positions[i];
        }
    });

    const pxr::UsdGeomPrimvarsAPI primvars(usd_mesh.GetPrim());
    pxr::VtArray<pxr::GfVec3f>    normals;
    pxr::TfToken                  normal_interpolation;
    if (auto normal_primvar = primvars.GetPrimvar(pxr::TfToken("normals")); normal_primvar) {
        normal_primvar.ComputeFlattened(&normals, pxr::UsdTimeCode::Default());
        normal_interpolation = normal_primvar.GetInterpolation();
    }

    if (normals.empty()) {
        usd_mesh.GetNormalsAttr().Get(&normals, pxr::UsdTimeCode::Default());
        normal_interpolation = usd_mesh.GetNormalsInterpolation();
    }

    if (!normals.empty()) {
        vertices->Modify<VertexAttribute::Normal>([&](auto values) {
            for (std::size_t i = 0; i < topology.point_indices.size(); ++i) {
                values[i] = ToVec3f(normals[SelectValueIndex(normal_interpolation, topology, i, normals.size())]);
            }
        });
    } else {
        auto computed_normals = ComputeFaceNormals(position_cpu);
        vertices->Modify<VertexAttribute::Normal>([&](auto values) {
            std::ranges::copy(computed_normals, values.begin());
        });
    }

    auto has_uv0 = false;
    if (auto st = primvars.GetPrimvar(pxr::TfToken("st")); st) {
        if (pxr::VtArray<pxr::GfVec2f> uvs; st.ComputeFlattened(&uvs, pxr::UsdTimeCode::Default()) && !uvs.empty()) {
            const auto interpolation = st.GetInterpolation();
            vertices->Modify<VertexAttribute::UV0>([&](auto values) {
                for (std::size_t i = 0; i < topology.point_indices.size(); ++i) {
                    values[i] = ToVec2f(uvs[SelectValueIndex(interpolation, topology, i, uvs.size())]);
                }
            });
            has_uv0 = true;
        }
    }
    if (!has_uv0) {
        vertices->Modify<VertexAttribute::UV0>([](auto values) {
            std::ranges::fill(values, vec2f{0.0f, 0.0f});
        });
    }

    if (auto display_color = primvars.GetPrimvar(pxr::TfToken("displayColor")); display_color) {
        if (pxr::VtArray<pxr::GfVec3f> colors; display_color.ComputeFlattened(&colors, pxr::UsdTimeCode::Default()) && !colors.empty()) {
            const auto interpolation = display_color.GetInterpolation();
            vertices->Modify<VertexAttribute::Color0>([&](auto values) {
                for (std::size_t i = 0; i < topology.point_indices.size(); ++i) {
                    values[i] = vec4f(ToColor(colors[SelectValueIndex(interpolation, topology, i, colors.size())]));
                }
            });
        }
    }

    indices->Modify<IndexType::UINT32>([](auto values) {
        std::iota(values.begin(), values.end(), std::uint32_t{0});
    });

    auto mesh = std::make_shared<Mesh>(vertices, indices, mesh_name);

    std::shared_ptr<Material> material;
    if (const auto usd_material = pxr::UsdShadeMaterialBindingAPI(usd_mesh.GetPrim()).ComputeBoundMaterial(); usd_material) {
        const auto material_key = usd_material.GetPath().GetString();
        auto       iter         = material_cache.find(material_key);
        if (iter == material_cache.end()) {
            auto material = BuildMaterialFromUsd(usd_material, source_asset, texture_cache, logger);
            if (material.material && material_processor) material_processor(material);
            if (cook_context) cook_context->SetMaterialSourceInfo(material.material, material.source_info);
            iter = material_cache.emplace(material_key, std::move(material)).first;
        }
        material = iter->second.material;
    }

    mesh->AddSubMesh({
        .index_count   = indices->Size(),
        .index_offset  = 0,
        .vertex_offset = 0,
        .material      = std::move(material),
    });
    return mesh;
}

auto BuildCamera(const pxr::UsdGeomCamera& usd_camera) -> std::shared_ptr<Camera> {
    auto gf_camera = usd_camera.GetCamera(pxr::UsdTimeCode::Default());
    auto clipping  = gf_camera.GetClippingRange();

    return std::make_shared<Camera>(
        Camera::Parameters{
            .aspect         = gf_camera.GetAspectRatio() > 0.0f ? gf_camera.GetAspectRatio() : 16.0f / 9.0f,
            .near_clip      = static_cast<float>(clipping.GetMin()),
            .far_clip       = static_cast<float>(clipping.GetMax()),
            .horizontal_fov = deg2rad(gf_camera.GetFieldOfView(pxr::GfCamera::FOVHorizontal)),
            .eye            = {0.0f, 0.0f, 0.0f},
            .look_dir       = {0.0f, 0.0f, -1.0f},
            .up             = {0.0f, 1.0f, 0.0f},
        },
        usd_camera.GetPrim().GetName().GetString());
}

auto BuildLight(const pxr::UsdPrim& prim, const mat4f& local_transform) -> std::shared_ptr<Light> {
    const pxr::UsdLuxLightAPI light_api(prim);

    Light::Parameters parameters;
    parameters.type      = prim.IsA<pxr::UsdLuxDistantLight>() ? Light::Type::Direction : Light::Type::Point;
    parameters.position  = get_translation(local_transform);
    parameters.direction = -get_forward(local_transform);
    parameters.up        = get_up(local_transform);

    if (float intensity = 1.0f; light_api.GetIntensityAttr().Get(&intensity, pxr::UsdTimeCode::Default())) {
        parameters.intensity = intensity;
    }
    if (pxr::GfVec3f color(1.0f, 1.0f, 1.0f); light_api.GetColorAttr().Get(&color, pxr::UsdTimeCode::Default())) {
        parameters.color = ToColor(color);
    }

    const pxr::UsdLuxShapingAPI shaping_api(prim);
    if (float cone_angle = 0.0f; shaping_api.GetShapingConeAngleAttr().Get(&cone_angle, pxr::UsdTimeCode::Default()) && cone_angle > 0.0f && cone_angle < 180.0f) {
        parameters.type             = Light::Type::Spot;
        parameters.outer_cone_angle = deg2rad(cone_angle);
        parameters.inner_cone_angle = parameters.outer_cone_angle;
    }

    return std::make_shared<Light>(parameters, prim.GetName().GetString());
}

bool IsUsdLight(const pxr::UsdPrim& prim) {
    return prim.IsA<pxr::UsdLuxBoundableLightBase>() ||
           prim.IsA<pxr::UsdLuxNonboundableLightBase>();
}

}  // namespace

auto GetEditorSceneFormat(std::string_view extension) noexcept -> EditorSceneFormat {
    if (!extension.empty() && extension.front() != '.') {
        const auto dot = extension.find_last_of('.');
        if (dot != std::string_view::npos) extension.remove_prefix(dot);
    }

    std::array<char, 16> lower{};
    const auto           size = std::min(extension.size(), lower.size() - 1);
    for (std::size_t i = 0; i < size; ++i) {
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(extension[i])));
    }
    const std::string_view ext(lower.data(), size);

    if (ext == ".usd") return EditorSceneFormat::USD;
    if (ext == ".usda") return EditorSceneFormat::USDA;
    if (ext == ".usdc") return EditorSceneFormat::USDC;
    if (ext == ".usdz") return EditorSceneFormat::USDZ;
    if (ext == ".hcscene" || ext == ".hitagiscene") return EditorSceneFormat::Cooked;
    return EditorSceneFormat::Unknown;
}

auto IsEditorScenePath(const std::filesystem::path& path) noexcept -> bool {
    return GetEditorSceneFormat(path.extension().string()) != EditorSceneFormat::Unknown;
}

UsdSceneImporter::UsdSceneImporter(
    MaterialGetter                  material_getter,
    std::shared_ptr<spdlog::logger> logger,
    MaterialProcessor               material_processor,
    EditorCookContext*              cook_context)
    : m_MaterialGetter(std::move(material_getter)),
      m_Logger(std::move(logger)),
      m_MaterialProcessor(std::move(material_processor)),
      m_CookContext(cook_context) {}

auto UsdSceneImporter::Import(
    const std::filesystem::path& path,
    const std::filesystem::path& resource_base_path) -> std::shared_ptr<Scene> {
    (void)resource_base_path;

    auto logger = m_Logger ? m_Logger : spdlog::default_logger();

    core::Clock clock;
    clock.Start();

    const auto usd_path = std::filesystem::absolute(path).generic_string();
    auto       stage    = pxr::UsdStage::Open(usd_path);
    if (!stage) {
        logger->error("Can not open USD stage: {}", usd_path);
        return nullptr;
    }

    auto                         scene = std::make_shared<Scene>(path.stem().string());
    MaterialCache                material_cache;
    TextureCache                 texture_cache;
    pxr::ArResolverContextBinder resolver_context(stage->GetPathResolverContext());

    std::function<void(const pxr::UsdPrim&, ecs::Entity)> convert = [&](const pxr::UsdPrim& prim, ecs::Entity parent) {
        if (!prim || !prim.IsActive() || prim.IsAbstract()) return;

        const auto local_transform = GetLocalTransform(prim);
        const auto name            = prim.GetName().GetString();

        ecs::Entity entity;
        if (prim.IsA<pxr::UsdGeomMesh>()) {
            auto mesh = BuildMesh(pxr::UsdGeomMesh(prim), material_cache, texture_cache, path, logger, m_MaterialProcessor, m_CookContext);
            entity    = mesh ? scene->CreateMeshEntity(std::move(mesh), local_transform, parent, name)
                             : scene->CreateEmptyEntity(local_transform, parent, name);
        } else if (prim.IsA<pxr::UsdGeomCamera>()) {
            entity = scene->CreateCameraEntity(BuildCamera(pxr::UsdGeomCamera(prim)), local_transform, parent, name);
        } else if (IsUsdLight(prim)) {
            entity = scene->CreateLightEntity(BuildLight(prim, local_transform), local_transform, parent, name);
        } else {
            entity = scene->CreateEmptyEntity(local_transform, parent, name);
        }

        for (const auto& child : prim.GetChildren()) convert(child, entity);
    };

    if (const auto default_prim = stage->GetDefaultPrim(); default_prim) {
        convert(default_prim, scene->GetRootEntity());
    } else {
        for (const auto& prim : stage->GetPseudoRoot().GetChildren()) convert(prim, scene->GetRootEntity());
    }

    logger->trace("USD scene parsed in {:.3}.", clock.TotalTime().count());
    return scene;
}

auto ImportEditorScene(
    asset::AssetManager&                asset_manager,
    const std::filesystem::path&        path,
    const std::filesystem::path&        resource_base_path,
    UsdSceneImporter::MaterialGetter    material_getter,
    std::shared_ptr<spdlog::logger>     logger,
    UsdSceneImporter::MaterialProcessor material_processor,
    EditorCookContext*                  cook_context) -> std::shared_ptr<Scene> {
    switch (GetEditorSceneFormat(path.extension().string())) {
        case EditorSceneFormat::USD:
        case EditorSceneFormat::USDA:
        case EditorSceneFormat::USDC:
        case EditorSceneFormat::USDZ:
            return UsdSceneImporter(std::move(material_getter), std::move(logger), std::move(material_processor), cook_context).Import(path, resource_base_path);
        case EditorSceneFormat::Cooked:
            return asset_manager.ImportScene(path, resource_base_path);
        case EditorSceneFormat::Unknown:
            return nullptr;
    }
    return nullptr;
}

}  // namespace hitagi
