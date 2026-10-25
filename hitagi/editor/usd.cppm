export module editor:usd;
import interop.usd;
import interop.spdlog;
import interop.magic_enum;

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
    // An empty name requests the host default for meshes without a bound material.
    using MaterialGetter    = std::function<std::shared_ptr<asset::Material>(std::string_view)>;
    using MaterialProcessor = std::function<void(EditorMaterial&)>;

    explicit UsdSceneImporter(
        MaterialGetter                  material_getter    = {},
        std::shared_ptr<spdlog::logger> logger             = nullptr,
        MaterialProcessor               material_processor = {},
        EditorCookContext*              cook_context       = nullptr);

    auto Import(
        core::JobSystem&             job_system,
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
    asset::AssetService&                asset_manager,
    core::JobSystem&                    job_system,
    const std::filesystem::path&        path,
    const std::filesystem::path&        resource_base_path = {},
    UsdSceneImporter::MaterialGetter    material_getter    = {},
    std::shared_ptr<spdlog::logger>     logger             = nullptr,
    UsdSceneImporter::MaterialProcessor material_processor = {},
    EditorCookContext*                  cook_context       = nullptr) -> std::shared_ptr<asset::Scene>;

}  // namespace hitagi

namespace hitagi {
namespace usd_import {

struct ExpandedMeshTopology {
    std::vector<std::uint32_t> point_indices;
    std::vector<std::size_t>   face_vertex_indices;
    std::vector<std::size_t>   face_indices;
};

using TextureCache  = std::pmr::unordered_map<std::string, std::shared_ptr<Texture>>;
using MaterialCache = std::pmr::unordered_map<std::string, EditorMaterial>;

static auto ToMat4f(const pxr::GfMatrix4d& matrix) noexcept -> mat4f {
    mat4f result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            // USD applies row vectors; engine transforms multiply column vectors.
            result[row][col] = static_cast<float>(matrix[col][row]);
        }
    }
    return result;
}

static auto ToVec2f(const pxr::GfVec2f& value) noexcept -> vec2f {
    return {value[0], value[1]};
}

static auto ToVec3f(const pxr::GfVec3f& value) noexcept -> vec3f {
    return {value[0], value[1], value[2]};
}

static auto ToVec4f(const pxr::GfVec4f& value) noexcept -> vec4f {
    return {value[0], value[1], value[2], value[3]};
}

static auto ToColor(const pxr::GfVec3f& value, float alpha = 1.0f) noexcept -> Color {
    return {value[0], value[1], value[2], alpha};
}

static auto GetLocalTransform(const pxr::UsdPrim& prim) -> mat4f {
    pxr::GfMatrix4d transform(1.0);
    bool            resets_xform_stack = false;

    const pxr::UsdGeomXformable xformable(prim);
    if (xformable) {
        xformable.GetLocalTransformation(&transform, &resets_xform_stack, pxr::UsdTimeCode(prim.GetStage()->GetStartTimeCode()));
    }

    return ToMat4f(transform);
}

static auto BuildExpandedTopology(
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

static auto SelectValueIndex(
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

static auto ComputeFaceNormals(const std::vector<vec3f>& positions) -> std::vector<vec3f> {
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

static auto DecodeUsdTexture(
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

    auto        image              = codec->DecodeImageData(buffer);
    const auto& resolved_path_text = resolved.GetPathString();
    auto        texture            = image ? std::make_shared<Texture>(std::move(image), authored_path.empty() ? resolved_path_text : authored_path, resolved_path_text) : nullptr;
    texture_cache.emplace(cache_key, texture);
    return texture;
}

static auto GetConnectedShader(
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

static bool InputHasConnection(const pxr::UsdShadeShader& shader, const pxr::TfToken& input_name) {
    const auto input = shader.GetInput(input_name);
    if (!input) return false;

    pxr::UsdShadeConnectableAPI source;
    pxr::TfToken                source_name;
    pxr::UsdShadeAttributeType  source_type = pxr::UsdShadeAttributeType::Invalid;
    return input.GetConnectedSource(&source, &source_name, &source_type);
}

static auto LoadConnectedTexture(
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

static void AppendTextureSource(MaterialSourceInfo& source_info, const std::shared_ptr<Texture>& texture) {
    if (texture && !texture->GetPath().empty()) source_info.source_texture_paths.emplace_back(texture->GetPath());
}

static void AppendUnsupportedConnection(
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

static auto ReadShaderInputValue(const pxr::UsdShadeInput& input) -> std::optional<MaterialParameterValue> {
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

static auto BuildMaterialFromUsd(
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

struct MeshData {
    VertexArray::Data         attributes;
    core::Buffer              indices;
    std::size_t               vertex_count;
    std::string               name;
    std::shared_ptr<Material> material;
};

static auto BuildMesh(
    const pxr::UsdGeomMesh&                    usd_mesh,
    MaterialCache&                             material_cache,
    TextureCache&                              texture_cache,
    const std::filesystem::path&               source_asset,
    const std::shared_ptr<spdlog::logger>&     logger,
    const UsdSceneImporter::MaterialProcessor& material_processor,
    EditorCookContext*                         cook_context) -> std::optional<MeshData> {
    pxr::VtArray<pxr::GfVec3f> points;
    pxr::VtArray<int>          face_vertex_counts;
    pxr::VtArray<int>          face_vertex_indices;

    if (!usd_mesh.GetPointsAttr().Get(&points, pxr::UsdTimeCode::Default()) ||
        !usd_mesh.GetFaceVertexCountsAttr().Get(&face_vertex_counts, pxr::UsdTimeCode::Default()) ||
        !usd_mesh.GetFaceVertexIndicesAttr().Get(&face_vertex_indices, pxr::UsdTimeCode::Default())) {
        return std::nullopt;
    }

    auto topology = BuildExpandedTopology(face_vertex_counts, face_vertex_indices);
    if (topology.point_indices.empty()) return std::nullopt;
    if (topology.point_indices.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("USD mesh is too large for uint32 index buffers");
    }

    const auto mesh_name = usd_mesh.GetPrim().GetName().GetString();
    MeshData   data{.vertex_count = topology.point_indices.size(), .name = mesh_name};
    auto       position_cpu = std::vector<vec3f>(topology.point_indices.size());

    {
        data.attributes[VertexAttribute::Position] = core::Buffer(data.vertex_count * sizeof(VertexDataType<VertexAttribute::Position>));
        auto positions                             = data.attributes[VertexAttribute::Position].Span<VertexDataType<VertexAttribute::Position>>();

        for (std::size_t i = 0; i < topology.point_indices.size(); ++i) {
            const auto point_index = std::min<std::size_t>(topology.point_indices[i], points.size() - 1);
            positions[i]           = ToVec3f(points[point_index]);
            position_cpu[i]        = positions[i];
        }
    }

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
        {
            data.attributes[VertexAttribute::Normal] = core::Buffer(data.vertex_count * sizeof(VertexDataType<VertexAttribute::Normal>));
            auto values                              = data.attributes[VertexAttribute::Normal].Span<VertexDataType<VertexAttribute::Normal>>();

            for (std::size_t i = 0; i < topology.point_indices.size(); ++i) {
                values[i] = ToVec3f(normals[SelectValueIndex(normal_interpolation, topology, i, normals.size())]);
            }
        }
    } else {
        auto computed_normals = ComputeFaceNormals(position_cpu);
        {
            data.attributes[VertexAttribute::Normal] = core::Buffer(data.vertex_count * sizeof(VertexDataType<VertexAttribute::Normal>));
            auto values                              = data.attributes[VertexAttribute::Normal].Span<VertexDataType<VertexAttribute::Normal>>();

            std::ranges::copy(computed_normals, values.begin());
        }
    }

    auto has_uv0 = false;
    if (auto st = primvars.GetPrimvar(pxr::TfToken("st")); st) {
        if (pxr::VtArray<pxr::GfVec2f> uvs; st.ComputeFlattened(&uvs, pxr::UsdTimeCode::Default()) && !uvs.empty()) {
            const auto interpolation = st.GetInterpolation();
            {
                data.attributes[VertexAttribute::UV0] = core::Buffer(data.vertex_count * sizeof(VertexDataType<VertexAttribute::UV0>));
                auto values                           = data.attributes[VertexAttribute::UV0].Span<VertexDataType<VertexAttribute::UV0>>();

                for (std::size_t i = 0; i < topology.point_indices.size(); ++i) {
                    values[i] = ToVec2f(uvs[SelectValueIndex(interpolation, topology, i, uvs.size())]);
                }
            }
            has_uv0 = true;
        }
    }
    if (!has_uv0) {
        {
            data.attributes[VertexAttribute::UV0] = core::Buffer(data.vertex_count * sizeof(VertexDataType<VertexAttribute::UV0>));
            auto values                           = data.attributes[VertexAttribute::UV0].Span<VertexDataType<VertexAttribute::UV0>>();

            std::ranges::fill(values, vec2f{0.0f, 0.0f});
        }
    }

    if (auto display_color = primvars.GetPrimvar(pxr::TfToken("displayColor")); display_color) {
        if (pxr::VtArray<pxr::GfVec3f> colors; display_color.ComputeFlattened(&colors, pxr::UsdTimeCode::Default()) && !colors.empty()) {
            const auto interpolation = display_color.GetInterpolation();
            {
                data.attributes[VertexAttribute::Color0] = core::Buffer(data.vertex_count * sizeof(VertexDataType<VertexAttribute::Color0>));
                auto values                              = data.attributes[VertexAttribute::Color0].Span<VertexDataType<VertexAttribute::Color0>>();

                for (std::size_t i = 0; i < topology.point_indices.size(); ++i) {
                    values[i] = vec4f(ToColor(colors[SelectValueIndex(interpolation, topology, i, colors.size())]));
                }
            }
        }
    }

    {
        data.indices = core::Buffer(data.vertex_count * sizeof(std::uint32_t));
        auto values  = data.indices.Span<std::uint32_t>();

        std::iota(values.begin(), values.end(), std::uint32_t{0});
    }

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

    data.material = std::move(material);
    return data;
}

static auto BuildCamera(const pxr::UsdGeomCamera& usd_camera) -> CameraComponent {
    auto gf_camera = usd_camera.GetCamera(pxr::UsdTimeCode::Default());
    auto clipping  = gf_camera.GetClippingRange();

    return CameraComponent{
        .aspect         = gf_camera.GetAspectRatio() > 0.0f ? gf_camera.GetAspectRatio() : 16.0f / 9.0f,
        .near_clip      = static_cast<float>(clipping.GetMin()),
        .far_clip       = static_cast<float>(clipping.GetMax()),
        .horizontal_fov = deg2rad(gf_camera.GetFieldOfView(pxr::GfCamera::FOVHorizontal)),
    };
}

static auto BuildLight(const pxr::UsdPrim& prim) -> LightComponent {
    const pxr::UsdLuxLightAPI light_api(prim);

    LightComponent parameters;
    parameters.type = prim.IsA<pxr::UsdLuxDistantLight>() ? LightComponent::Type::Direction : LightComponent::Type::Point;

    if (float intensity = 1.0f; light_api.GetIntensityAttr().Get(&intensity, pxr::UsdTimeCode::Default())) {
        parameters.intensity = intensity;
    }
    if (pxr::GfVec3f color(1.0f, 1.0f, 1.0f); light_api.GetColorAttr().Get(&color, pxr::UsdTimeCode::Default())) {
        parameters.color = ToColor(color);
    }

    const pxr::UsdLuxShapingAPI shaping_api(prim);
    if (float cone_angle = 0.0f; shaping_api.GetShapingConeAngleAttr().Get(&cone_angle, pxr::UsdTimeCode::Default()) && cone_angle > 0.0f && cone_angle < 180.0f) {
        parameters.type             = LightComponent::Type::Spot;
        parameters.outer_cone_angle = deg2rad(cone_angle);
        parameters.inner_cone_angle = parameters.outer_cone_angle;
    }

    return parameters;
}

static bool IsUsdLight(const pxr::UsdPrim& prim) {
    return prim.IsA<pxr::UsdLuxBoundableLightBase>() ||
           prim.IsA<pxr::UsdLuxNonboundableLightBase>();
}

static auto UsdResourceKey(const pxr::UsdPrim& prim) -> std::string {
    return (prim.IsInstanceProxy() ? prim.GetPrimInPrototype() : prim).GetPath().GetString();
}

static auto BuildAnimationClip(const pxr::UsdSkelAnimQuery& query, double start, double end, double rate,
                               AnimationInterpolation interpolation) -> std::shared_ptr<AnimationClip> {
    const auto          order = query.GetJointOrder();
    std::vector<double> samples;
    if (!query.GetJointTransformTimeSamples(&samples)) throw std::runtime_error("Cannot read USD animation samples");
    std::erase_if(samples, [&](double time) { return time < start || time > end; });
    samples.emplace_back(start);
    samples.emplace_back(end);
    std::ranges::sort(samples);
    samples.erase(std::unique(samples.begin(), samples.end()), samples.end());
    std::vector<AnimationTrack> tracks(order.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        tracks[i].target                  = static_cast<std::uint32_t>(i);
        tracks[i].positions.interpolation = interpolation;
        tracks[i].rotations.interpolation = interpolation;
        tracks[i].scales.interpolation    = interpolation;
    }
    for (double sample : samples) {
        pxr::VtArray<pxr::GfVec3f> translations;
        pxr::VtArray<pxr::GfQuatf> rotations;
        pxr::VtArray<pxr::GfVec3h> scales;
        if (!query.ComputeJointLocalTransformComponents(&translations, &rotations, &scales, pxr::UsdTimeCode(sample)) ||
            translations.size() != order.size() || rotations.size() != order.size() || scales.size() != order.size())
            throw std::runtime_error(std::format("Invalid joint animation arrays at {} ({})", query.GetPrim().GetPath().GetString(), sample));
        const double time = (sample - start) / rate;
        for (std::size_t i = 0; i < order.size(); ++i) {
            const auto imaginary = rotations[i].GetImaginary();
            tracks[i].positions.keys.emplace_back(time, ToVec3f(translations[i]));
            tracks[i].rotations.keys.emplace_back(time, quatf(imaginary[0], imaginary[1], imaginary[2], rotations[i].GetReal()));
            tracks[i].scales.keys.emplace_back(time, vec3f(float(scales[i][0]), float(scales[i][1]), float(scales[i][2])));
        }
    }
    return std::make_shared<AnimationClip>((end - start) / rate, std::move(tracks), query.GetPrim().GetName().GetString());
}

struct ImportedSkeleton {
    std::vector<ecs::Entity>                     joints;
    std::unordered_map<std::string, std::size_t> joint_indices;
    pxr::VtArray<pxr::GfMatrix4d>                bind_transforms;
};

// USD objects and caches stay inside authoring. The returned scene owns only
// runtime assets and entity references, and never needs a live UsdStage.
static void ImportSkeletons(Scene& scene, const std::vector<std::pair<pxr::UsdPrim, ecs::Entity>>& prim_entities, std::unordered_map<ecs::Entity, MeshData>& mesh_data) {
    if (prim_entities.empty()) return;
    const auto        stage = prim_entities.front().first.GetStage();
    pxr::UsdSkelCache cache;
    for (const auto& [prim, entity] : prim_entities) {
        if (prim.IsA<pxr::UsdSkelRoot>() && !cache.Populate(pxr::UsdSkelRoot(prim), pxr::UsdTraverseInstanceProxies()))
            throw std::runtime_error("Cannot populate USD skeleton bindings: " + prim.GetPath().GetString());
    }

    const double rate = stage->GetTimeCodesPerSecond();
    if (!std::isfinite(rate) || rate <= 0.0) throw std::runtime_error("USD timeCodesPerSecond must be positive");
    double start = stage->GetStartTimeCode(), end = stage->GetEndTimeCode();
    if (!stage->HasAuthoredTimeCodeRange()) {
        std::vector<double> times;
        for (const auto& [prim, entity] : prim_entities) {
            if (!prim.IsA<pxr::UsdSkelSkeleton>()) continue;
            const auto          query = cache.GetSkelQuery(pxr::UsdSkelSkeleton(prim));
            std::vector<double> samples;
            if (query && query.GetAnimQuery()) query.GetAnimQuery().GetJointTransformTimeSamples(&samples);
            times.insert(times.end(), samples.begin(), samples.end());
        }
        if (!times.empty()) {
            const auto range = std::ranges::minmax(times);
            start            = range.min;
            end              = range.max;
        }
    }
    if (!std::isfinite(start) || !std::isfinite(end) || end < start) throw std::runtime_error("Invalid USD animation time range");

    std::unordered_map<std::string, ImportedSkeleton>               skeletons;
    std::unordered_map<std::string, std::shared_ptr<AnimationClip>> clips;
    for (auto [prim, entity] : prim_entities) {
        if (!prim.IsA<pxr::UsdSkelSkeleton>()) continue;
        const auto path = prim.GetPath().GetString();
        for (auto ancestor = prim; ancestor && !ancestor.IsPseudoRoot(); ancestor = ancestor.GetParent()) {
            const pxr::UsdGeomXformable xform(ancestor);
            if (xform && (xform.TransformMightBeTimeVarying() || xform.GetResetXformStack()))
                throw std::runtime_error("Animated/reset USD skeleton ancestor transforms are not supported: " + ancestor.GetPath().GetString());
        }
        const auto  query = cache.GetSkelQuery(pxr::UsdSkelSkeleton(prim));
        std::string reason;
        if (!query || !query.GetTopology().Validate(&reason)) throw std::runtime_error("Invalid USD skeleton " + path + ": " + reason);
        auto&                         skeleton = skeletons[path];
        const auto                    order    = query.GetJointOrder();
        pxr::VtArray<pxr::GfMatrix4d> local_transforms;
        if (!query.ComputeJointLocalTransforms(&local_transforms, pxr::UsdTimeCode(start)) || local_transforms.size() != order.size() ||
            !query.GetJointWorldBindTransforms(&skeleton.bind_transforms) || skeleton.bind_transforms.size() != order.size())
            throw std::runtime_error("USD skeleton requires valid local and bind transforms: " + path);
        const auto group = scene.GetWorld().GetEntityManager().Create(asset::Transform(mat4f::identity()), asset::RelationShip(entity), asset::MetaInfo("__joints"));
        for (std::size_t i = 0; i < order.size(); ++i) {
            const auto name = order[i].GetString();
            if (!skeleton.joint_indices.emplace(name, i).second) throw std::runtime_error("Duplicate USD joint: " + name);
            const int parent = query.GetTopology().GetParent(i);
            skeleton.joints.emplace_back(scene.GetWorld().GetEntityManager().Create(asset::Transform(ToMat4f(local_transforms[i])), asset::RelationShip(parent < 0 ? group : skeleton.joints.at(parent)), asset::MetaInfo(name.substr(name.find_last_of('/') + 1))));
        }
        if (const auto& animation = query.GetAnimQuery(); animation && !animation.GetJointOrder().empty()) {
            const auto key  = UsdResourceKey(animation.GetPrim());
            auto&      clip = clips[key];
            if (!clip) clip = BuildAnimationClip(animation, start, end, rate,
                                                 stage->GetInterpolationType() == pxr::UsdInterpolationTypeHeld ? AnimationInterpolation::Step : AnimationInterpolation::Linear);
            entity.Emplace<AnimationPlayer>(clip);
            const auto                      animated_order = animation.GetJointOrder();
            std::unordered_set<std::string> targets;
            for (std::size_t i = 0; i < animated_order.size(); ++i) {
                const auto name = animated_order[i].GetString();
                if (!targets.insert(name).second) throw std::runtime_error("Duplicate USD animation joint: " + name);
                if (const auto found = skeleton.joint_indices.find(name); found != skeleton.joint_indices.end())
                    skeleton.joints[found->second].Emplace<AnimationTarget>(entity, static_cast<std::uint32_t>(i));
            }
        }
    }

    for (auto [prim, entity] : prim_entities) {
        if (prim.GetAttribute(pxr::TfToken("hitagi:spring:joints")))
            throw std::runtime_error("Legacy spring metadata is unsupported; re-export an authored hitagi:rig rig");
        const auto header = prim.GetAttribute(pxr::TfToken("hitagi:rig:bodies"));
        if (!header) continue;
        const auto found = skeletons.find(prim.GetPath().GetString());
        if (found == skeletons.end()) throw std::runtime_error("Rigid rig metadata requires Skeleton");
        pxr::VtArray<int>             body_ids, joint_ids;
        pxr::VtArray<float>           properties, limits;
        pxr::VtArray<pxr::GfMatrix4d> bind, offsets, frames;
        if (!header.Get(&body_ids) || body_ids.size() % 5 != 0 ||
            !prim.GetAttribute(pxr::TfToken("hitagi:rig:properties")).Get(&properties) ||
            !prim.GetAttribute(pxr::TfToken("hitagi:rig:bind")).Get(&bind) ||
            !prim.GetAttribute(pxr::TfToken("hitagi:rig:offsets")).Get(&offsets) ||
            !prim.GetAttribute(pxr::TfToken("hitagi:rig:joints")).Get(&joint_ids) ||
            !prim.GetAttribute(pxr::TfToken("hitagi:rig:frames")).Get(&frames) ||
            !prim.GetAttribute(pxr::TfToken("hitagi:rig:limits")).Get(&limits) ||
            properties.size() != body_ids.size() / 5 * 8 || bind.size() != body_ids.size() / 5 ||
            offsets.size() != bind.size() || joint_ids.size() % 2 != 0 ||
            frames.size() != joint_ids.size() / 2 || limits.size() != frames.size() * 18)
            throw std::runtime_error("Invalid rigid rig array sizes");
        for (float value : properties)
            if (!std::isfinite(value)) throw std::runtime_error("Nonfinite rig property");
        for (float value : limits)
            if (!std::isfinite(value)) throw std::runtime_error("Nonfinite rig limit");
        auto& rig  = entity.Emplace<physics::CharacterPhysics>();
        rig.joints = found->second.joints;
        for (unsigned i = 0; i < bind.size(); ++i) {
            const auto* ids = body_ids.data() + i * 5;
            const auto* p   = properties.data() + i * 8;
            if (ids[0] < -1 || ids[0] >= static_cast<int>(rig.joints.size()) ||
                ids[3] <= 0 || ids[3] > 65535 || ids[4] < 0 || ids[4] > 65535 ||
                p[4] < 0 || p[4] > 1 || p[5] < 0 || p[5] > 1 || p[6] < 0 || p[7] < 0)
                throw std::runtime_error("Invalid rigid rig body properties");
            rig.bodies.push_back({.joint = ids[0], .mode = ids[1], .shape = ids[2], .size = vec3f(p[0], p[1], p[2]), .bind = ToMat4f(bind[i]), .bone_offset = ToMat4f(offsets[i]), .mass = p[3], .linear_damping = p[4], .angular_damping = p[5], .friction = p[6], .restitution = p[7], .group = static_cast<unsigned>(ids[3]), .mask = static_cast<unsigned>(ids[4])});
        }
        for (unsigned i = 0; i < frames.size(); ++i) {
            const auto* p = limits.data() + i * 18;
            rig.constraints.push_back({.a              = static_cast<unsigned>(joint_ids[i * 2]),
                                       .b              = static_cast<unsigned>(joint_ids[i * 2 + 1]),
                                       .bind           = ToMat4f(frames[i]),
                                       .linear_min     = vec3f(p[0], p[1], p[2]),
                                       .linear_max     = vec3f(p[3], p[4], p[5]),
                                       .angular_min    = vec3f(p[6], p[7], p[8]),
                                       .angular_max    = vec3f(p[9], p[10], p[11]),
                                       .linear_spring  = vec3f(p[12], p[13], p[14]),
                                       .angular_spring = vec3f(p[15], p[16], p[17])});
        }
    }

    // Mesh prototypes may be shared, but the joint list belongs to each instance.
    // Compare influence streams before deduplicating: inherited primvars can differ.
    std::unordered_map<std::string, std::vector<std::shared_ptr<Skin>>> skins;
    for (auto [prim, entity] : prim_entities) {
        if (!mesh_data.contains(entity)) continue;
        const auto query = cache.GetSkinningQuery(prim);
        if (query && query.HasBlendShapes()) throw std::runtime_error("USD blend shapes are not supported: " + prim.GetPath().GetString());
        if (!query || !query.HasJointInfluences()) {
            const pxr::UsdGeomPrimvarsAPI primvars(prim);
            if (primvars.FindPrimvarWithInheritance(pxr::TfToken("skel:jointIndices")).HasValue() ||
                primvars.FindPrimvarWithInheritance(pxr::TfToken("skel:jointWeights")).HasValue())
                throw std::runtime_error("Missing or invalid USD skin influences/SkelRoot: " + prim.GetPath().GetString());
            continue;
        }
        const auto path = prim.GetPath().GetString();
        if (query.GetSkinningMethod().GetString() != "classicLinear") throw std::runtime_error("Only classicLinear USD skinning is supported: " + path);
        pxr::VtArray<pxr::TfToken> blend_shapes;
        if (query.GetBlendShapeOrder(&blend_shapes) && !blend_shapes.empty()) throw std::runtime_error("USD blend shapes are not supported: " + path);
        std::vector<double> binding_times;
        query.GetTimeSamples(&binding_times);
        if (binding_times.size() > 1) throw std::runtime_error("Animated USD skin bindings are not supported: " + path);
        const auto bound = pxr::UsdSkelBindingAPI(prim).GetInheritedSkeleton();
        const auto found = skeletons.find(bound.GetPath().GetString());
        if (found == skeletons.end()) throw std::runtime_error("Bound USD skeleton is outside the imported scene: " + path);
        const auto&                skeleton = found->second;
        pxr::VtArray<pxr::TfToken> joint_order;
        if (!query.GetJointOrder(&joint_order)) joint_order = cache.GetSkelQuery(bound).GetJointOrder();
        std::vector<ecs::Entity> joints;
        std::vector<mat4f>       inverse_bind;
        const auto               geom_bind = query.GetGeomBindTransform(pxr::UsdTimeCode(start));
        for (const auto& joint : joint_order) {
            const auto index = skeleton.joint_indices.find(joint.GetString());
            if (index == skeleton.joint_indices.end()) throw std::runtime_error("Unknown USD skin joint: " + joint.GetString());
            joints.emplace_back(skeleton.joints[index->second]);
            const auto bind = ToMat4f(skeleton.bind_transforms[index->second]);
            if (determinant(bind) == 0.0f) throw std::runtime_error("Singular USD bind transform: " + path);
            // Column-vector equivalent of USD's geomBind * inverse(bind).
            inverse_bind.emplace_back(inverse(bind) * ToMat4f(geom_bind));
        }
        if (joints.empty()) throw std::runtime_error("USD skin has an empty joint palette: " + path);
        const pxr::UsdGeomMesh     usd_mesh(prim);
        pxr::VtArray<pxr::GfVec3f> points;
        pxr::VtArray<int>          counts, face_indices, joint_indices;
        pxr::VtArray<float>        weights;
        usd_mesh.GetPointsAttr().Get(&points);
        usd_mesh.GetFaceVertexCountsAttr().Get(&counts);
        usd_mesh.GetFaceVertexIndicesAttr().Get(&face_indices);
        if (usd_mesh.GetPointsAttr().ValueMightBeTimeVarying() ||
            usd_mesh.GetFaceVertexCountsAttr().ValueMightBeTimeVarying() || usd_mesh.GetFaceVertexIndicesAttr().ValueMightBeTimeVarying())
            throw std::runtime_error("Animated USD mesh points/topology are not supported: " + path);
        if (points.empty() || std::ranges::any_of(face_indices, [&](int index) { return index < 0 || std::size_t(index) >= points.size(); }))
            throw std::runtime_error("Invalid USD skinned mesh point indices: " + path);
        const auto topology = BuildExpandedTopology(counts, face_indices);
        if (!query.ComputeVaryingJointInfluences(points.size(), &joint_indices, &weights, pxr::UsdTimeCode(start)))
            throw std::runtime_error("Invalid USD skin influences: " + path);
        const int influences = query.GetNumInfluencesPerComponent();
        if (influences <= 0 || weights.size() != points.size() * std::size_t(influences) || joint_indices.size() != weights.size())
            throw std::runtime_error("USD skin influence count mismatch: " + path);
        std::vector<vec4u> indices_per_point(points.size(), vec4u(0));
        std::vector<vec4f> weights_per_point(points.size(), vec4f(0.0f));
        for (std::size_t point = 0; point < points.size(); ++point) {
            unsigned used  = 0;
            double   total = 0.0;
            for (int influence = 0; influence < influences; ++influence) {
                const auto offset = point * influences + influence;
                const auto weight = weights[offset];
                if (!std::isfinite(weight) || weight < 0.0f) throw std::runtime_error("Invalid USD skin weight: " + path);
                if (weight == 0.0f) continue;
                if (used == 4) throw std::runtime_error("USD vertex has more than four nonzero joint influences: " + path);
                if (joint_indices[offset] < 0 || std::size_t(joint_indices[offset]) >= joints.size()) throw std::runtime_error("USD joint index out of range: " + path);
                indices_per_point[point][used]   = joint_indices[offset];
                weights_per_point[point][used++] = weight;
                total += weight;
            }
            // USD points must reach skeleton space. Runtime's unweighted-vertex
            // convention is mesh-local, so reject zero sums rather than disagree.
            if (total == 0.0) throw std::runtime_error("USD skin vertex has no nonzero weights: " + path);
            for (unsigned i = 0; i < used; ++i) weights_per_point[point][i] = float(weights_per_point[point][i] / total);
        }
        auto& data = mesh_data.at(entity);
        {
            data.attributes[VertexAttribute::BlendIndex] = core::Buffer(data.vertex_count * sizeof(VertexDataType<VertexAttribute::BlendIndex>));
            auto values                                  = data.attributes[VertexAttribute::BlendIndex].Span<VertexDataType<VertexAttribute::BlendIndex>>();

            for (std::size_t i = 0; i < topology.point_indices.size(); ++i) values[i] = indices_per_point.at(topology.point_indices[i]);
        }
        {
            data.attributes[VertexAttribute::BlendWeight] = core::Buffer(data.vertex_count * sizeof(VertexDataType<VertexAttribute::BlendWeight>));
            auto values                                   = data.attributes[VertexAttribute::BlendWeight].Span<VertexDataType<VertexAttribute::BlendWeight>>();

            for (std::size_t i = 0; i < topology.point_indices.size(); ++i) values[i] = weights_per_point.at(topology.point_indices[i]);
        }
        const auto key             = UsdResourceKey(prim);
        auto&      skin_candidates = skins[key];
        auto       skin_found      = std::ranges::find_if(skin_candidates, [&](const auto& skin) {
            return std::ranges::equal(skin->GetInverseBindMatrices(), inverse_bind);
        });
        auto       skin            = skin_found == skin_candidates.end() ? skin_candidates.emplace_back(std::make_shared<Skin>(std::move(inverse_bind), prim.GetName().GetString())) : *skin_found;
        entity.Emplace<SkinnedMesh>(skin, std::move(joints));
    }
}

}  // namespace usd_import

using namespace usd_import;

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
    core::JobSystem&             job_system,
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

    auto                                              scene = std::make_shared<Scene>(path.stem().string());
    MaterialCache                                     material_cache;
    TextureCache                                      texture_cache;
    pxr::ArResolverContextBinder                      resolver_context(stage->GetPathResolverContext());
    std::vector<std::pair<pxr::UsdPrim, ecs::Entity>> prim_entities;
    std::unordered_map<ecs::Entity, MeshData>         mesh_data;

    std::function<void(const pxr::UsdPrim&, ecs::Entity)> convert = [&](const pxr::UsdPrim& prim, ecs::Entity parent) {
        if (!prim || !prim.IsActive() || prim.IsAbstract()) return;

        const auto local_transform = GetLocalTransform(prim);
        const auto name            = prim.GetName().GetString();

        auto entity = scene->GetWorld().GetEntityManager().Create(
            Transform(local_transform), RelationShip(parent), MetaInfo(name));
        if (prim.IsA<pxr::UsdGeomMesh>()) {
            auto mesh = BuildMesh(pxr::UsdGeomMesh(prim), material_cache, texture_cache, path, logger, m_MaterialProcessor, m_CookContext);
            if (mesh) {
                mesh_data.emplace(entity, std::move(*mesh));
            }
        } else if (prim.IsA<pxr::UsdGeomCamera>()) {
            entity.Emplace<CameraComponent>(BuildCamera(pxr::UsdGeomCamera(prim)));
        } else if (IsUsdLight(prim)) {
            entity.Emplace<LightComponent>(BuildLight(prim));
        }

        prim_entities.emplace_back(prim, entity);
        for (const auto& child : prim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies())) convert(child, entity);
    };

    if (const auto default_prim = stage->GetDefaultPrim(); default_prim) {
        convert(default_prim, {});
    } else {
        for (const auto& prim : stage->GetPseudoRoot().GetChildren()) convert(prim, {});
    }

    ImportSkeletons(*scene, prim_entities, mesh_data);
    // Publish once, after inherited skin influences and materials are resolved.
    std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>> meshes;
    for (auto [prim, entity] : prim_entities) {
        const auto found = mesh_data.find(entity);
        if (found == mesh_data.end()) continue;
        auto& data = found->second;
        if (!data.material && m_MaterialGetter) data.material = m_MaterialGetter({});
        auto&      candidates = meshes[UsdResourceKey(prim)];
        const auto shared     = std::ranges::find_if(candidates, [&](const std::shared_ptr<Mesh>& mesh) {
            if (mesh->vertices->Size() != data.vertex_count || mesh->sub_meshes.front().material != data.material ||
                !std::ranges::equal(mesh->indices->GetIndexData().cpu_buffer.Span<const std::byte>(), data.indices.Span<const std::byte>())) return false;
            bool equal = true;
            magic_enum::enum_for_each<VertexAttribute>([&](VertexAttribute attr) {
                const auto attribute = mesh->vertices->GetAttributeData(attr);
                if (attribute)
                    equal &= std::ranges::equal(attribute->get().cpu_buffer.Span<const std::byte>(), data.attributes[attr].Span<const std::byte>());
                else
                    equal &= data.attributes[attr].Empty();
            });
            return equal;
        });
        auto       mesh       = shared != candidates.end() ? *shared : candidates.emplace_back(std::make_shared<Mesh>(std::make_shared<VertexArray>(data.vertex_count, std::move(data.attributes), data.name + "-vertices"), std::make_shared<IndexArray>(IndexType::UINT32, std::move(data.indices), data.name + "-indices"), std::pmr::vector<Mesh::SubMesh>{{.index_count = data.vertex_count, .material = data.material}}, data.name));
        entity.Emplace<MeshComponent>(std::move(mesh));
    }
    if (!scene->GetWorld().GetEntityManager().GetEntities<physics::CharacterPhysics>().empty())
        scene->GetWorld().RegisterSystem<physics::CharacterPhysicsSystem>();
    scene->GetWorld().Update(job_system);
    logger->trace("USD scene parsed in {:.3}.", clock.TotalTime().count());
    return scene;
}

auto ImportEditorScene(
    asset::AssetService&                asset_manager,
    core::JobSystem&                    job_system,
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
            return UsdSceneImporter(std::move(material_getter), std::move(logger), std::move(material_processor), cook_context).Import(job_system, path, resource_base_path);
        case EditorSceneFormat::Cooked:
            return asset_manager.ImportScene(path, resource_base_path);
        case EditorSceneFormat::Unknown:
            return nullptr;
    }
    return nullptr;
}

}  // namespace hitagi
