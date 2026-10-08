export module pbr_demo_game;

import engine;
import std;

export namespace game::pbr_demo {

inline constexpr std::string_view kPbrForwardContract = "DemoPBRForward";

struct PbrMaterialDesc {
    hitagi::math::Color                     base_color = hitagi::math::Color::White();
    std::shared_ptr<hitagi::asset::Texture> base_color_texture;
    float                                   metallic  = 0.0f;
    float                                   roughness = 0.5f;
    std::shared_ptr<hitagi::asset::Texture> metallic_roughness_texture;
    std::shared_ptr<hitagi::asset::Texture> normal_texture;
    float                                   occlusion = 1.0f;
    std::shared_ptr<hitagi::asset::Texture> occlusion_texture;
    hitagi::math::Color                     emissive_color = hitagi::math::Color::Black();
    std::shared_ptr<hitagi::asset::Texture> emissive_texture;
    float                                   opacity      = 1.0f;
    bool                                    alpha_cutout = false;
    float                                   alpha_cutoff = 0.5f;
};

auto CreatePbrMaterialParameters(const PbrMaterialDesc& desc) -> hitagi::asset::MaterialParameters;

auto DefaultSourceScenePath() -> std::filesystem::path;
auto DefaultCookedScenePath() -> std::filesystem::path;
auto FindDefaultSourceScenePath() -> std::filesystem::path;

void ConfigurePbrDemoScene(hitagi::asset::Scene& scene);

class PbrDemoRenderer final : public hitagi::render::IRenderer {
public:
    explicit PbrDemoRenderer(const hitagi::asset::ResourceLoadContext& load_context);

    auto Render(hitagi::render::RenderContext& context, const hitagi::render::RenderRequest& request) -> hitagi::render::RenderResult final;

private:
    auto CountRenderableSubMeshes(const hitagi::render::RenderRequest& request) const -> std::size_t;

    hitagi::gfx::Device&                  m_Device;
    hitagi::asset::ResourceLoadContext    m_LoadContext;
    std::shared_ptr<hitagi::gfx::Sampler> m_Sampler;
};

class PbrDemoGame final : public hitagi::core::RuntimeModule {
public:
    explicit PbrDemoGame(hitagi::Engine& engine, std::filesystem::path cooked_scene_path = DefaultCookedScenePath());

    void Tick() final;

private:
    void LoadSceneIfNeeded();
    auto BuildView(std::uint32_t width, std::uint32_t height) -> hitagi::render::RenderView;
    void CollectFrameData();

    hitagi::Engine&                                  m_Engine;
    std::filesystem::path                            m_CookedScenePath;
    std::shared_ptr<hitagi::asset::Scene>            m_Scene;
    std::pmr::vector<hitagi::render::RenderDrawItem> m_DrawItems;
    std::pmr::vector<hitagi::render::RenderLight>    m_Lights;
};

}  // namespace game::pbr_demo

namespace game::pbr_demo {
namespace {

using namespace hitagi;

struct PbrFrameConstant {
    math::vec4f camera_pos;
    math::mat4f view;
    math::mat4f projection;
    math::mat4f proj_view;
    math::mat4f inv_view;
    math::mat4f inv_projection;
    math::mat4f inv_proj_view;
    math::vec4f light_position;
    math::vec4f light_pos_in_view;
    math::vec3f light_color;
    float       light_intensity = 1.0f;
};

struct PbrInstanceConstant {
    math::mat4f model;
};

struct PbrBindlessInfo {
    gfx::BindlessHandle frame_constant;
    gfx::BindlessHandle instance_constant;
    std::uint32_t       instance_index  = 0;
    std::uint32_t       instance_stride = 0;
    gfx::BindlessHandle material_data;
    gfx::BindlessHandle sampler;
};

struct PbrDemoDrawRecord {
    asset::Mesh::SubMesh                                           sub_mesh;
    std::shared_ptr<asset::Material>                               material;
    core::Buffer                                                   material_data;
    rg::GPUBufferEdgeHandle                                        material_access;
    std::pmr::vector<std::pair<std::uint8_t, rg::GPUBufferHandle>> vertex_buffers;
    rg::GPUBufferHandle                                            indices;
    rg::GPUBufferHandle                                            material_data_handle;
    std::shared_ptr<gfx::RenderPipeline>                           pipeline;
    std::size_t                                                    instance_index = 0;
};

bool ConfigurePbrDemoMaterial(const std::shared_ptr<asset::Material>& material) {
    if (!material) return false;
    const auto* pass = material->FindPass(kPbrForwardContract);
    return pass != nullptr && pass->pipeline != nullptr;
}

void UploadMaterialData(
    const asset::MaterialPass& material_pass,
    gfx::GPUBuffer&            material_data_buffer) {
    const auto& material_data = material_pass.material_data;
    if (material_data.Empty()) return;

    auto* const mapped_data = static_cast<std::byte*>(material_data_buffer.Map());
    std::memcpy(mapped_data, material_data.GetData(), material_data.GetDataSize());
    material_data_buffer.UnMap();
}

auto BuildRenderView(const asset::Camera& camera, const math::mat4f& camera_transform) noexcept -> render::RenderView {
    const math::vec3f global_eye      = (camera_transform * math::vec4f(camera.parameters.eye, 1.0f)).xyz;
    const math::vec3f global_look_dir = (camera_transform * math::vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const math::vec3f global_up       = (camera_transform * math::vec4f(camera.parameters.up, 0.0f)).xyz;
    return {
        .camera_position = global_eye,
        .view            = math::look_at(global_eye, global_look_dir, global_up),
        .projection      = math::perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip),
    };
}

auto BuildDefaultDemoView(float aspect) noexcept -> render::RenderView {
    const auto eye      = math::vec3f{0.0f, -3.0f, 1.6f};
    const auto look_dir = math::vec3f{0.0f, 1.0f, -0.35f};
    const auto up       = math::vec3f{0.0f, 0.0f, 1.0f};
    return {
        .camera_position = eye,
        .view            = math::look_at(eye, look_dir, up),
        .projection      = math::perspective(60.0_deg, aspect, 0.1f, 1000.0f),
    };
}

auto MakeFrameConstant(const render::RenderRequest& request) -> PbrFrameConstant {
    const auto light = request.frame.lights.empty()
                           ? render::RenderLight{.position = {3.0f, -4.0f, 4.0f}, .color = math::Color::White(), .intensity = 30.0f}
                           : request.frame.lights.front();

    PbrFrameConstant frame{
        .camera_pos      = math::vec4f(request.frame.view.camera_position, 1.0f),
        .view            = request.frame.view.view,
        .projection      = request.frame.view.projection,
        .proj_view       = request.frame.view.projection * request.frame.view.view,
        .inv_view        = math::inverse(request.frame.view.view),
        .inv_projection  = math::inverse(request.frame.view.projection),
        .inv_proj_view   = math::inverse(request.frame.view.projection * request.frame.view.view),
        .light_position  = math::vec4f(light.position, 1.0f),
        .light_color     = {light.color[0], light.color[1], light.color[2]},
        .light_intensity = light.intensity,
    };
    frame.light_pos_in_view = frame.view * frame.light_position;
    return frame;
}

auto CreateFrameTarget(render::RenderContext& context, const render::RenderRequest& request) -> rg::TextureHandle {
    if (context.graph.IsValid(request.target)) return request.target;

    const auto width  = context.swap_chain ? context.swap_chain->GetWidth() : 1u;
    const auto height = context.swap_chain ? context.swap_chain->GetHeight() : 1u;
    return context.graph.Create(gfx::TextureDesc{
        .name        = "PbrDemoColor",
        .width       = width,
        .height      = height,
        .format      = gfx::Format::R8G8B8A8_UNORM,
        .clear_value = math::Color{0.02f, 0.025f, 0.03f, 1.0f},
        .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopySrc,
    });
}

auto CreateDepthTarget(render::RenderContext& context, const gfx::TextureDesc& color_desc) -> rg::TextureHandle {
    return context.graph.Create(gfx::TextureDesc{
        .name        = "PbrDemoDepth",
        .width       = color_desc.width,
        .height      = color_desc.height,
        .format      = gfx::Format::D32_FLOAT,
        .clear_value = gfx::ClearDepthStencil{.depth = 1.0f},
        .usages      = gfx::TextureUsageFlags::DepthStencil | gfx::TextureUsageFlags::SRV,
    });
}

auto IsRenderablePbrMaterial(const std::shared_ptr<asset::Material>& material) noexcept -> bool {
    if (!material) return false;
    const auto* pass = material->FindPass(kPbrForwardContract);
    return pass != nullptr && pass->pipeline != nullptr;
}

auto CreatePbrDemoMaterialPipeline(const asset::ResourceLoadContext& load_context, const asset::MaterialPass& pass) -> std::shared_ptr<gfx::RenderPipeline> {
    if (!pass.pipeline) return nullptr;
    pass.pipeline->Load(load_context);
    return pass.pipeline->GetBuiltPipeline();
}

}  // namespace

auto CreatePbrMaterialParameters(const PbrMaterialDesc& desc) -> hitagi::asset::MaterialParameters {
    return hitagi::asset::MaterialParameters{
        {.name = "base_color", .value = desc.base_color},
        {.name = "metallic", .value = desc.metallic},
        {.name = "roughness", .value = desc.roughness},
        {.name = "occlusion", .value = desc.occlusion},
        {.name = "emissive_color", .value = desc.emissive_color},
        {.name = "opacity", .value = desc.opacity},
        {.name = "alpha_cutout", .value = static_cast<std::uint32_t>(desc.alpha_cutout ? 1u : 0u)},
        {.name = "alpha_cutoff", .value = desc.alpha_cutoff},
        {.name = "base_color_texture", .value = desc.base_color_texture},
        {.name = "metallic_roughness_texture", .value = desc.metallic_roughness_texture},
        {.name = "normal_texture", .value = desc.normal_texture},
        {.name = "occlusion_texture", .value = desc.occlusion_texture},
        {.name = "emissive_texture", .value = desc.emissive_texture},
    };
}

auto DefaultSourceScenePath() -> std::filesystem::path {
    return "assets/test/test.usda";
}

auto DefaultCookedScenePath() -> std::filesystem::path {
    return "assets/cooked/pbr_demo_scene.hcscene";
}

auto FindDefaultSourceScenePath() -> std::filesystem::path {
    static constexpr std::array candidates{
        std::string_view{"assets/DamagedHelmet/DamagedHelmet.usda"},
        std::string_view{"assets/DamagedHelmet/damaged_helmet.usda"},
        std::string_view{"assets/damaged_helmet/DamagedHelmet.usda"},
        std::string_view{"assets/test/pbr_texture_golden.usda"},
        std::string_view{"assets/test/test.usda"},
    };
    for (const auto candidate : candidates) {
        if (std::filesystem::exists(candidate)) return std::filesystem::path(candidate);
    }
    return DefaultSourceScenePath();
}

void ConfigurePbrDemoScene(asset::Scene& scene) {
    for (const auto entity : scene.GetMeshEntities()) {
        if (!entity || !entity.Has<asset::MeshComponent>()) continue;
        auto mesh = entity.Get<asset::MeshComponent>().mesh;
        if (!mesh) continue;
        for (auto& sub_mesh : mesh->sub_meshes) {
            if (!sub_mesh.material) {
                throw std::runtime_error(std::format("PBR demo mesh '{}' has a submesh without material", mesh->GetName()));
            }
            if (!ConfigurePbrDemoMaterial(sub_mesh.material)) {
                throw std::runtime_error(std::format("PBR demo material '{}' is missing cooked DemoPBRForward pass", sub_mesh.material->GetName()));
            }
        }
    }
}

PbrDemoRenderer::PbrDemoRenderer(const asset::ResourceLoadContext& load_context)
    : IRenderer("PbrDemoRenderer"),
      m_Device(load_context.device),
      m_LoadContext(load_context),
      m_Sampler(hitagi::gfx::Sampler::Create(m_Device, m_LoadContext.bindings, {
                                                                                   .name          = "PbrDemoSampler",
                                                                                   .address_u     = gfx::AddressMode::Repeat,
                                                                                   .address_v     = gfx::AddressMode::Repeat,
                                                                                   .address_w     = gfx::AddressMode::Repeat,
                                                                                   .mag_filter    = gfx::FilterMode::Linear,
                                                                                   .min_filter    = gfx::FilterMode::Linear,
                                                                                   .mipmap_filter = gfx::FilterMode::Linear,
                                                                               })) {}

auto PbrDemoRenderer::CountRenderableSubMeshes(const render::RenderRequest& request) const -> std::size_t {
    std::size_t count = 0;
    for (const auto& item : request.frame.draw_items) {
        if (!item.mesh || item.mesh->Empty()) continue;
        for (const auto& sub_mesh : item.mesh->sub_meshes) {
            if (IsRenderablePbrMaterial(sub_mesh.material)) ++count;
        }
    }
    return count;
}

auto PbrDemoRenderer::Render(render::RenderContext& context, const render::RenderRequest& request) -> render::RenderResult {
    auto color = CreateFrameTarget(context, request);
    if (!context.graph.IsValid(color)) return {};

    const auto color_desc = context.graph.GetResourceDesc(color);
    auto       depth      = CreateDepthTarget(context, color_desc);
    const auto draw_count = CountRenderableSubMeshes(request);
    if (draw_count == 0) {
        return {.color = color, .depth = depth};
    }

    for (const auto& item : request.frame.draw_items) {
        if (item.mesh) item.mesh->Load(m_LoadContext);
        if (!item.mesh) continue;
        for (const auto& sub_mesh : item.mesh->sub_meshes) {
            if (sub_mesh.material) sub_mesh.material->Load(m_LoadContext);
        }
    }

    auto frame_constant    = context.graph.Create({
        .name   = "PbrDemoFrameConstant",
        .size   = utils::align(sizeof(PbrFrameConstant), gfx::GPUBuffer::GetStorageViewRequirements(context.graph.GetDevice()).size_alignment),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });
    auto instance_constant = context.graph.Create({
        .name   = "PbrDemoInstanceConstant",
        .size   = sizeof(PbrInstanceConstant) * std::max<std::size_t>(1, request.frame.draw_items.size()),
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });
    auto bindless_info     = context.graph.Create({
        .name   = "PbrDemoBindlessInfo",
        .size   = sizeof(PbrBindlessInfo) * draw_count,
        .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
    });

    std::pmr::vector<PbrDemoDrawRecord> draw_records;
    draw_records.reserve(draw_count);
    std::size_t instance_index = 0;
    for (const auto& item : request.frame.draw_items) {
        if (!item.mesh || item.mesh->Empty()) {
            ++instance_index;
            continue;
        }
        for (const auto& sub_mesh : item.mesh->sub_meshes) {
            if (!IsRenderablePbrMaterial(sub_mesh.material)) continue;

            sub_mesh.material->Load(m_LoadContext);
            const auto* material_pass = sub_mesh.material->FindPass(kPbrForwardContract);
            if (!material_pass) continue;

            auto material_pipeline = CreatePbrDemoMaterialPipeline(m_LoadContext, *material_pass);
            if (!material_pipeline) continue;

            auto material_data = material_pass->material_data;

            auto material_data_handle = context.graph.Create(
                {
                    .name   = std::pmr::string(std::format("PbrDemoMaterialData-{}", draw_records.size())),
                    .size   = utils::align(material_data.GetDataSize(), gfx::GPUBuffer::GetStorageViewRequirements(context.graph.GetDevice()).size_alignment),
                    .usages = gfx::GPUBufferUsageFlags::StorageRead | gfx::GPUBufferUsageFlags::MapWrite,
                });

            draw_records.emplace_back(PbrDemoDrawRecord{
                .sub_mesh             = sub_mesh,
                .material             = sub_mesh.material,
                .material_data        = std::move(material_data),
                .material_data_handle = material_data_handle,
                .pipeline             = std::move(material_pipeline),
                .instance_index       = instance_index,
            });
            auto& record = draw_records.back();

            for (const auto& vertex_attr : record.pipeline->GetDesc().vertex_input_layout) {
                const auto mesh_attr = asset::semantic_to_vertex_attribute(vertex_attr.semantic);
                if (const auto attr = item.mesh->vertices->GetAttributeData(mesh_attr); attr && attr->get().gpu_buffer) {
                    record.vertex_buffers.emplace_back(vertex_attr.binding, context.graph.Import(attr->get().gpu_buffer));
                }
            }
            record.indices = context.graph.Import(item.mesh->indices->GetGPUData());
        }
        ++instance_index;
    }

    auto                  sampler = context.graph.Import(m_Sampler);
    rg::RenderPassBuilder builder(context.graph);
    builder.SetName(std::format("PbrDemoForwardPass-{}", context.graph.GetFrameIndex()));
    builder.SetRenderTarget(color, true);
    builder.SetDepthStencil(depth, true);
    const auto frame_constant_access    = builder.Read(frame_constant, {.offset = 0, .element_size = sizeof(PbrFrameConstant), .element_count = 1}, gfx::PipelineStage::VertexShader | gfx::PipelineStage::PixelShader);
    const auto instance_constant_access = builder.Read(instance_constant, {.offset = 0, .element_size = sizeof(PbrInstanceConstant), .element_count = std::max<std::size_t>(1, request.frame.draw_items.size())}, gfx::PipelineStage::VertexShader);
    const auto bindless_info_access     = builder.Read(bindless_info, {.offset = 0, .element_size = sizeof(PbrBindlessInfo), .element_count = draw_count}, gfx::PipelineStage::All);
    builder.AddSampler(sampler);

    for (auto& record : draw_records) {
        record.material_access = builder.Read(record.material_data_handle);
        for (const auto [_, vertex_buffer] : record.vertex_buffers) {
            builder.ReadAsVertices(vertex_buffer);
        }
        builder.ReadAsIndices(record.indices);
    }

    std::pmr::vector<math::mat4f> instance_transforms;
    instance_transforms.reserve(request.frame.draw_items.size());
    for (const auto& item : request.frame.draw_items) instance_transforms.emplace_back(item.transform);

    builder.SetExecutor([=, this, records = std::move(draw_records), transforms = std::move(instance_transforms), frame_data = MakeFrameConstant(request)](const rg::RenderGraph&, const rg::RenderPassNode& pass) mutable {
        pass.Resolve(frame_constant_access).GetMappedSpan<PbrFrameConstant>().front() = frame_data;

        auto instance_constants = pass.Resolve(instance_constant_access).GetMappedSpan<PbrInstanceConstant>();
        for (std::size_t i = 0; i < transforms.size(); ++i) {
            instance_constants[i] = {.model = transforms[i]};
        }

        auto        bindless_infos = pass.Resolve(bindless_info_access).GetMappedSpan<PbrBindlessInfo>();
        auto&       cmd            = pass.GetCmd();
        const auto& target         = pass.Resolve(color);
        cmd.SetViewPort({
            .x      = 0.0f,
            .y      = 0.0f,
            .width  = static_cast<float>(target.GetDesc().width),
            .height = static_cast<float>(target.GetDesc().height),
        });
        cmd.SetScissorRect({
            .x      = 0,
            .y      = 0,
            .width  = target.GetDesc().width,
            .height = target.GetDesc().height,
        });
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto& record               = records[i];
            auto&       material_data_buffer = pass.Resolve(record.material_data_handle);
            const auto* material_pass        = record.material ? record.material->FindPass(kPbrForwardContract) : nullptr;
            if (!material_pass) continue;
            UploadMaterialData(
                *material_pass,
                material_data_buffer);

            auto& bindless = bindless_infos[i];
            bindless       = {
                .frame_constant    = pass.Resolve(frame_constant_access).GetBindlessHandle(),
                .instance_constant = pass.Resolve(instance_constant_access).GetBindlessHandle(),
                .instance_index    = static_cast<std::uint32_t>(record.instance_index),
                .instance_stride   = static_cast<std::uint32_t>(pass.Resolve(instance_constant_access).GetDesc().element_stride),
                .material_data     = pass.Resolve(record.material_access).GetBindlessHandle(),
                .sampler           = pass.Resolve(sampler).GetBindlessHandle(),
            };

            cmd.PushBindlessMetaInfo({
                .handle        = pass.Resolve(bindless_info_access).GetBindlessHandle(),
                .record_index  = static_cast<std::uint32_t>(i),
                .record_stride = static_cast<std::uint32_t>(pass.Resolve(bindless_info_access).GetDesc().element_stride),
            });

            cmd.SetPipeline(*record.pipeline);
            for (const auto [binding, vertex_buffer] : record.vertex_buffers) {
                cmd.SetVertexBuffers(binding, {{pass.Resolve(vertex_buffer)}}, {{0}});
            }
            cmd.SetIndexBuffer(pass.Resolve(record.indices), 0);
            cmd.DrawIndexed(record.sub_mesh.index_count, 1, record.sub_mesh.index_offset, record.sub_mesh.vertex_offset);
        }
    });
    builder.Finish();

    return {.color = color, .depth = depth};
}

PbrDemoGame::PbrDemoGame(hitagi::Engine& engine, std::filesystem::path cooked_scene_path)
    : RuntimeModule("PbrDemoGame"),
      m_Engine(engine),
      m_CookedScenePath(std::move(cooked_scene_path)) {}

void PbrDemoGame::LoadSceneIfNeeded() {
    if (m_Scene) return;
    if (!std::filesystem::exists(m_CookedScenePath)) {
        throw std::runtime_error(std::format("PBR demo cooked scene not found: {}", m_CookedScenePath.string()));
    }

    m_Scene = m_Engine.Assets().ImportScene(m_CookedScenePath);
    if (!m_Scene) {
        throw std::runtime_error(std::format("PBR demo failed to load cooked scene: {}", m_CookedScenePath.string()));
    }
    try {
        ConfigurePbrDemoScene(*m_Scene);
    } catch (const std::exception& error) {
        m_Scene.reset();
        throw std::runtime_error(std::format("PBR demo cooked scene is incompatible: {}", error.what()));
    }
    m_Logger->info("Loaded cooked scene: {}", m_CookedScenePath.string());
}

auto PbrDemoGame::BuildView(std::uint32_t width, std::uint32_t height) -> render::RenderView {
    const auto aspect = height == 0 ? 1.0f : static_cast<float>(width) / static_cast<float>(height);
    if (!m_Scene) return BuildDefaultDemoView(aspect);

    if (auto camera_entity = m_Scene->GetCurrentCamera(); camera_entity && camera_entity.Has<asset::CameraComponent>() && camera_entity.Has<asset::Transform>()) {
        auto camera = camera_entity.Get<asset::CameraComponent>().camera;
        if (camera) {
            camera->parameters.aspect = aspect;
            return BuildRenderView(*camera, camera_entity.Get<asset::Transform>().world_matrix);
        }
    }
    return BuildDefaultDemoView(aspect);
}

void PbrDemoGame::CollectFrameData() {
    m_DrawItems.clear();
    m_Lights.clear();
    if (!m_Scene) return;

    m_Scene->Update(m_Engine.Jobs());
    m_DrawItems.reserve(m_Scene->GetMeshEntities().size());
    m_Lights.reserve(m_Scene->GetLightEntities().size());

    for (const auto entity : m_Scene->GetMeshEntities()) {
        if (!entity || !entity.Has<asset::MeshComponent>() || !entity.Has<asset::Transform>()) continue;
        auto mesh = entity.Get<asset::MeshComponent>().mesh;
        if (!mesh || mesh->Empty()) continue;
        m_DrawItems.emplace_back(render::RenderDrawItem{
            .mesh      = std::move(mesh),
            .transform = entity.Get<asset::Transform>().world_matrix,
            .object_id = static_cast<std::uint32_t>(entity.GetId() + 1u),
        });
    }

    for (const auto entity : m_Scene->GetLightEntities()) {
        if (!entity || !entity.Has<asset::LightComponent>() || !entity.Has<asset::Transform>()) continue;
        auto light = entity.Get<asset::LightComponent>().light;
        if (!light) continue;
        m_Lights.emplace_back(render::RenderLight{
            .position  = (entity.Get<asset::Transform>().world_matrix * math::vec4f(light->parameters.position, 1.0f)).xyz,
            .color     = light->parameters.color,
            .intensity = std::max(light->parameters.intensity, 1.0f),
        });
    }
}

void PbrDemoGame::Tick() {
    LoadSceneIfNeeded();
    CollectFrameData();

    if (!m_Engine.App().WindowsMinimized()) {
        auto&      runtime = m_Engine.RenderRuntime();
        auto&      graph   = runtime.GetRenderGraph();
        const auto width   = runtime.GetSwapChain().GetWidth();
        const auto height  = runtime.GetSwapChain().GetHeight();

        auto output = graph.Create(gfx::TextureDesc{
            .name        = "PbrDemoGameOutput",
            .width       = width,
            .height      = height,
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::Color{0.02f, 0.025f, 0.03f, 1.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopySrc,
        });

        auto context = runtime.MakeContext();
        output       = m_Engine.Renderer().Render(
                                              context,
                                              render::RenderRequest{
                                                  .frame = render::RenderFrame{
                                                      .view       = BuildView(width, height),
                                                      .draw_items = m_DrawItems,
                                                      .lights     = m_Lights,
                                                  },
                                                  .target = output,
                                              })
                           .color;

        runtime.ToSwapChain(output);
    }
}

}  // namespace game::pbr_demo
