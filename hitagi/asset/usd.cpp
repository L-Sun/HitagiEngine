module;

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/range1f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/ar/resolverContextBinder.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvar.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdLux/boundableLightBase.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdLux/nonboundableLightBase.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/tokens.h>
#include <spdlog/spdlog.h>

module asset;
import std;

using namespace hitagi::math;

namespace hitagi::asset {
namespace {

struct ExpandedMeshTopology {
    std::vector<std::uint32_t> point_indices;
    std::vector<std::size_t>   face_vertex_indices;
    std::vector<std::size_t>   face_indices;
};

using TextureCache = std::pmr::unordered_map<std::string, std::shared_ptr<Texture>>;

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

auto BuildExpandedTopology(const pxr::VtArray<int>& face_vertex_counts,
                           const pxr::VtArray<int>& face_vertex_indices) -> ExpandedMeshTopology {
    ExpandedMeshTopology topology;

    std::size_t face_vertex_offset = 0;
    for (std::size_t face_index = 0; face_index < face_vertex_counts.size(); ++face_index) {
        const int face_vertex_count = face_vertex_counts[face_index];
        if (face_vertex_count < 3) {
            face_vertex_offset += std::max(face_vertex_count, 0);
            continue;
        }
        if (face_vertex_offset + static_cast<std::size_t>(face_vertex_count) > face_vertex_indices.size()) {
            break;
        }

        for (int vertex = 1; vertex + 1 < face_vertex_count; ++vertex) {
            const std::array<std::size_t, 3> fan_indices = {
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

auto SelectValueIndex(const pxr::TfToken& interpolation,
                      const ExpandedMeshTopology& topology,
                      std::size_t expanded_index,
                      std::size_t value_count) -> std::size_t {
    if (value_count == 0) return 0;
    if (value_count == topology.point_indices.size()) {
        return expanded_index;
    }

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
        if (normal.norm() > std::numeric_limits<float>::epsilon()) {
            normal = normalize(normal);
        } else {
            normal = {0.0f, 0.0f, 1.0f};
        }

        normals[i] = normals[i + 1] = normals[i + 2] = normal;
    }

    return normals;
}

template <typename T>
bool ReadShaderInput(const pxr::UsdShadeShader& shader, const pxr::TfToken& name, T* value) {
    const auto input = shader.GetInput(name);
    return input.IsDefined() && input.Get(value, pxr::UsdTimeCode::Default());
}

auto DecodeUsdTexture(const pxr::SdfAssetPath& asset_path,
                      TextureCache& texture_cache,
                      const std::shared_ptr<spdlog::logger>& logger) -> std::shared_ptr<Texture> {
    const auto authored_path = asset_path.GetAssetPath();
    const auto resolved_path = asset_path.GetResolvedPath();
    const auto cache_key     = !resolved_path.empty() ? resolved_path : authored_path;
    if (cache_key.empty()) {
        return nullptr;
    }

    if (const auto iter = texture_cache.find(cache_key); iter != texture_cache.end()) {
        return iter->second;
    }

    auto& resolver = pxr::ArGetResolver();
    auto  resolved = !resolved_path.empty()
                         ? pxr::ArResolvedPath(resolved_path)
                         : resolver.Resolve(authored_path);
    if (!resolved) {
        if (logger) logger->warn("Failed to resolve USD texture asset: {}", authored_path);
        texture_cache.emplace(cache_key, nullptr);
        return nullptr;
    }

    auto asset = resolver.OpenAsset(resolved);
    if (!asset) {
        if (logger) logger->warn("Failed to open USD texture asset: {}", resolved.GetPathString());
        texture_cache.emplace(cache_key, nullptr);
        return nullptr;
    }

    const auto size = asset->GetSize();
    core::Buffer buffer(size);
    if (asset->Read(buffer.GetData(), size, 0) != size) {
        if (logger) logger->warn("Failed to read USD texture asset: {}", resolved.GetPathString());
        texture_cache.emplace(cache_key, nullptr);
        return nullptr;
    }

    const auto image_format = get_image_format(authored_path);
    std::shared_ptr<ImageDecoder> decoder;
    switch (image_format) {
        case ImageFormat::PNG:
            decoder = std::make_shared<PngDecoder>(logger);
            break;
        case ImageFormat::JPEG:
            decoder = std::make_shared<JpegDecoder>(logger);
            break;
        case ImageFormat::TGA:
            decoder = std::make_shared<TgaDecoder>(logger);
            break;
        case ImageFormat::BMP:
            decoder = std::make_shared<BmpDecoder>(logger);
            break;
        case ImageFormat::UNKOWN:
            if (logger) logger->warn("Unsupported USD texture format: {}", authored_path);
            texture_cache.emplace(cache_key, nullptr);
            return nullptr;
    }

    auto texture = decoder->Decode(buffer);
    if (texture) {
        texture->SetName(authored_path.empty() ? resolved.GetPathString() : authored_path);
        texture->SetPath(authored_path);
    }
    texture_cache.emplace(cache_key, texture);
    return texture;
}

auto GetConnectedShader(const pxr::UsdShadeShader& shader,
                        const pxr::TfToken& input_name) -> std::optional<pxr::UsdShadeShader> {
    const auto input = shader.GetInput(input_name);
    if (!input) {
        return std::nullopt;
    }

    pxr::UsdShadeConnectableAPI source;
    pxr::TfToken                source_name;
    pxr::UsdShadeAttributeType  source_type = pxr::UsdShadeAttributeType::Invalid;
    if (!input.GetConnectedSource(&source, &source_name, &source_type)) {
        return std::nullopt;
    }

    auto source_shader = pxr::UsdShadeShader(source.GetPrim());
    if (!source_shader) {
        return std::nullopt;
    }
    return source_shader;
}

auto LoadConnectedTexture(const pxr::UsdShadeShader& shader,
                          const pxr::TfToken& input_name,
                          TextureCache& texture_cache,
                          const std::shared_ptr<spdlog::logger>& logger) -> std::shared_ptr<Texture> {
    const auto connected_shader = GetConnectedShader(shader, input_name);
    if (!connected_shader) {
        return nullptr;
    }

    pxr::TfToken shader_id;
    if (!connected_shader->GetIdAttr().Get(&shader_id) || shader_id != pxr::TfToken("UsdUVTexture")) {
        return nullptr;
    }

    const auto file_input = connected_shader->GetInput(pxr::TfToken("file"));
    if (!file_input) {
        return nullptr;
    }

    pxr::SdfAssetPath texture_path;
    if (!file_input.Get(&texture_path, pxr::UsdTimeCode::Default())) {
        return nullptr;
    }

    return DecodeUsdTexture(texture_path, texture_cache, logger);
}

auto BuildMaterialInstance(const pxr::UsdShadeMaterial& usd_material,
                           const std::function<std::shared_ptr<asset::Material>(std::string_view)>& material_getter,
                           TextureCache& texture_cache,
                           const std::shared_ptr<spdlog::logger>& logger) -> std::shared_ptr<MaterialInstance> {
    auto material_instance = std::make_shared<MaterialInstance>();
    if (usd_material) {
        material_instance->SetName(usd_material.GetPath().GetName());
    }

    if (material_getter) {
        material_instance->SetMaterial(material_getter("Phong"));
    }

    if (!usd_material) {
        return material_instance;
    }

    const auto shader = usd_material.ComputeSurfaceSource(pxr::TfTokenVector{pxr::UsdShadeTokens->universalRenderContext});
    if (!shader) {
        return material_instance;
    }

    if (pxr::GfVec3f diffuse; ReadShaderInput(shader, pxr::TfToken("diffuseColor"), &diffuse)) {
        material_instance->SetParameter("diffuse", ToColor(diffuse));
    }
    if (pxr::GfVec3f specular; ReadShaderInput(shader, pxr::TfToken("specularColor"), &specular)) {
        material_instance->SetParameter("specular", ToColor(specular));
    }
    if (pxr::GfVec3f emissive; ReadShaderInput(shader, pxr::TfToken("emissiveColor"), &emissive)) {
        material_instance->SetParameter("emissive", ToColor(emissive));
    }
    if (float roughness = 0.5f; ReadShaderInput(shader, pxr::TfToken("roughness"), &roughness)) {
        material_instance->SetParameter("roughness", std::clamp(roughness, 0.04f, 1.0f));
        material_instance->SetParameter("shininess", std::max(1.0f, (1.0f - roughness) * 128.0f));
    }
    if (float metallic = 0.0f; ReadShaderInput(shader, pxr::TfToken("metallic"), &metallic)) {
        material_instance->SetParameter("metallic", std::clamp(metallic, 0.0f, 1.0f));
    }
    if (float occlusion = 1.0f; ReadShaderInput(shader, pxr::TfToken("occlusion"), &occlusion)) {
        material_instance->SetParameter("occlusion", std::clamp(occlusion, 0.0f, 1.0f));
    }

    if (auto texture = LoadConnectedTexture(shader, pxr::TfToken("diffuseColor"), texture_cache, logger)) {
        material_instance->SetParameter("diffuse_texture", std::move(texture));
    }
    if (auto texture = LoadConnectedTexture(shader, pxr::TfToken("emissiveColor"), texture_cache, logger)) {
        material_instance->SetParameter("emissive_texture", std::move(texture));
    }
    if (auto texture = LoadConnectedTexture(shader, pxr::TfToken("normal"), texture_cache, logger)) {
        material_instance->SetParameter("normal_texture", std::move(texture));
    }
    if (auto texture = LoadConnectedTexture(shader, pxr::TfToken("occlusion"), texture_cache, logger)) {
        material_instance->SetParameter("occlusion_texture", std::move(texture));
    }
    if (auto texture = LoadConnectedTexture(shader, pxr::TfToken("roughness"), texture_cache, logger)) {
        material_instance->SetParameter("metallic_roughness_texture", std::move(texture));
    } else if (auto texture = LoadConnectedTexture(shader, pxr::TfToken("metallic"), texture_cache, logger)) {
        material_instance->SetParameter("metallic_roughness_texture", std::move(texture));
    }

    return material_instance;
}

auto BuildFallbackMaterialInstance(const std::function<std::shared_ptr<asset::Material>(std::string_view)>& material_getter) -> std::shared_ptr<MaterialInstance> {
    auto material_instance = std::make_shared<MaterialInstance>();
    if (material_getter) {
        material_instance->SetMaterial(material_getter("Phong"));
    }
    return material_instance;
}

auto BuildMesh(const pxr::UsdGeomMesh& usd_mesh,
               const std::function<std::shared_ptr<asset::Material>(std::string_view)>& material_getter,
               std::pmr::unordered_map<std::string, std::shared_ptr<MaterialInstance>>& material_cache,
               TextureCache& texture_cache,
               const std::shared_ptr<spdlog::logger>& logger) -> std::shared_ptr<Mesh> {
    pxr::VtArray<pxr::GfVec3f> points;
    pxr::VtArray<int>          face_vertex_counts;
    pxr::VtArray<int>          face_vertex_indices;

    if (!usd_mesh.GetPointsAttr().Get(&points, pxr::UsdTimeCode::Default()) ||
        !usd_mesh.GetFaceVertexCountsAttr().Get(&face_vertex_counts, pxr::UsdTimeCode::Default()) ||
        !usd_mesh.GetFaceVertexIndicesAttr().Get(&face_vertex_indices, pxr::UsdTimeCode::Default())) {
        return nullptr;
    }

    auto topology = BuildExpandedTopology(face_vertex_counts, face_vertex_indices);
    if (topology.point_indices.empty()) {
        return nullptr;
    }

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
            positions[i]          = ToVec3f(points[point_index]);
            position_cpu[i]       = positions[i];
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

    auto                          has_uv0 = false;
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
                    values[i] = math::vec4f(ToColor(colors[SelectValueIndex(interpolation, topology, i, colors.size())]));
                }
            });
        }
    }

    indices->Modify<IndexType::UINT32>([](auto values) {
        std::iota(values.begin(), values.end(), std::uint32_t{0});
    });

    auto mesh = std::make_shared<Mesh>(vertices, indices, mesh_name);

    auto material_instance = BuildFallbackMaterialInstance(material_getter);
    if (const auto usd_material = pxr::UsdShadeMaterialBindingAPI(usd_mesh.GetPrim()).ComputeBoundMaterial(); usd_material) {
        const auto material_key = usd_material.GetPath().GetString();
        auto       iter         = material_cache.find(material_key);
        if (iter == material_cache.end()) {
            iter = material_cache.emplace(material_key, BuildMaterialInstance(usd_material, material_getter, texture_cache, logger)).first;
        }
        material_instance = iter->second;
    }

    mesh->AddSubMesh({
        .index_count       = indices->Size(),
        .index_offset      = 0,
        .vertex_offset     = 0,
        .material_instance = material_instance,
    });

    return mesh;
}

auto BuildCamera(const pxr::UsdGeomCamera& usd_camera) -> std::shared_ptr<Camera> {
    auto gf_camera = usd_camera.GetCamera(pxr::UsdTimeCode::Default());
    auto clipping  = gf_camera.GetClippingRange();

    return std::make_shared<Camera>(
        Camera::Parameters{
            .aspect         = gf_camera.GetAspectRatio() > 0.0f ? gf_camera.GetAspectRatio() : 16.0f / 9.0f,
            .near_clip      = clipping.GetMin(),
            .far_clip       = clipping.GetMax(),
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

auto UsdParser::Parse(const std::filesystem::path& path, const std::filesystem::path& resource_base_path) -> std::shared_ptr<Scene> {
    (void)resource_base_path;

    auto logger = m_Logger ? m_Logger : spdlog::default_logger();

    core::Clock clock;
    clock.Start();

    const auto usd_path = std::filesystem::absolute(path).generic_string();

    auto stage = pxr::UsdStage::Open(usd_path);
    if (!stage) {
        logger->error("Can not open USD stage: {}", usd_path);
        return nullptr;
    }

    auto scene = std::make_shared<Scene>(path.stem().string());
    std::pmr::unordered_map<std::string, std::shared_ptr<MaterialInstance>> material_cache;
    TextureCache texture_cache;
    pxr::ArResolverContextBinder resolver_context(stage->GetPathResolverContext());

    std::function<void(const pxr::UsdPrim&, ecs::Entity)> convert = [&](const pxr::UsdPrim& prim, ecs::Entity parent) {
        if (!prim || !prim.IsActive() || prim.IsAbstract()) return;

        const auto local_transform = GetLocalTransform(prim);
        const auto name            = prim.GetName().GetString();

        ecs::Entity entity;
        if (prim.IsA<pxr::UsdGeomMesh>()) {
            auto mesh = BuildMesh(pxr::UsdGeomMesh(prim), m_MaterialGetter, material_cache, texture_cache, logger);
            entity    = mesh ? scene->CreateMeshEntity(std::move(mesh), local_transform, parent, name)
                             : scene->CreateEmptyEntity(local_transform, parent, name);
        } else if (prim.IsA<pxr::UsdGeomCamera>()) {
            entity = scene->CreateCameraEntity(BuildCamera(pxr::UsdGeomCamera(prim)), local_transform, parent, name);
        } else if (IsUsdLight(prim)) {
            entity = scene->CreateLightEntity(BuildLight(prim, local_transform), local_transform, parent, name);
        } else {
            entity = scene->CreateEmptyEntity(local_transform, parent, name);
        }

        for (const auto& child : prim.GetChildren()) {
            convert(child, entity);
        }
    };

    if (const auto default_prim = stage->GetDefaultPrim(); default_prim) {
        convert(default_prim, scene->GetRootEntity());
    } else {
        for (const auto& prim : stage->GetPseudoRoot().GetChildren()) {
            convert(prim, scene->GetRootEntity());
        }
    }

    logger->trace("USD scene parsed in {:.3}.", clock.TotalTime().count());
    return scene;
}

}  // namespace hitagi::asset
