module;

#include "interop/tracy_macros.hpp"

#undef near
#undef far

module render;
import interop.tracy;
import interop.magic_enum;
import std;

namespace hitagi::render {
namespace {

constexpr auto ToIndexFormat(asset::IndexType type) noexcept -> gfx::Format {
    return type == asset::IndexType::UINT16 ? gfx::Format::R16_UINT : gfx::Format::R32_UINT;
}

}  // namespace

auto LoadShaderSource(core::FileIOManager& file_io, std::filesystem::path path) -> ShaderSource {
    auto code = std::pmr::string(file_io.SyncOpenAndReadBinary(path).Str());
    return {.path = std::move(path), .code = std::move(code)};
}

auto DeferredRenderer::Render(RenderContext& context, const RenderRequest& request) -> RenderResult {
    if (!context.graph.IsValid(request.target)) {
        return RenderResult{.color = request.target};
    }
    return RenderFrame(context, request);
}

void DeferredRenderer::AddExtension(std::shared_ptr<IDeferredRenderExtension> extension) {
    if (extension) m_Extensions.emplace_back(std::move(extension));
}

void DeferredRenderer::ClearExtensions() {
    m_Extensions.clear();
}

DeferredRenderer::DeferredRenderer(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, core::FileIOManager& file_io, const Application& app, std::string_view name)
    : IRenderer(std::format("DeferredRenderer{}", name.empty() ? "" : std::format("({})", name))),
      m_App(app),
      m_GfxDevice(device),
      m_Queues(queues),
      m_Bindings(bindings),
      m_ShaderCompiler(compiler),
      m_LoadContext{.device = device, .queues = queues, .bindings = bindings, .shader_compiler = compiler},
      m_PersistentSampler(hitagi::gfx::Sampler::Create(m_GfxDevice, m_Bindings, {
                                                                                    .name          = "sampler",
                                                                                    .address_u     = gfx::AddressMode::Repeat,
                                                                                    .address_v     = gfx::AddressMode::Repeat,
                                                                                    .address_w     = gfx::AddressMode::Repeat,
                                                                                    .mag_filter    = gfx::FilterMode::Linear,
                                                                                    .min_filter    = gfx::FilterMode::Linear,
                                                                                    .mipmap_filter = gfx::FilterMode::Linear,
                                                                                })),
      m_GBufferPass(m_GfxDevice, bindings, compiler, LoadShaderSource(file_io, m_App.GetConfig().asset_root_path / "shaders" / "deferred_gbuffer.hlsl")),
      m_DeferredLightingPass(m_GfxDevice, bindings, compiler, LoadShaderSource(file_io, m_App.GetConfig().asset_root_path / "shaders" / "deferred_lighting.hlsl")),
      m_GBufferDebugViewPass(m_GfxDevice, bindings, compiler, LoadShaderSource(file_io, m_App.GetConfig().asset_root_path / "shaders" / "deferred_debug.hlsl")) {}

auto DeferredRenderer::RenderFrame(RenderContext& context, const RenderRequest& request) -> RenderResult {
    ZoneScoped;
    auto&       render_graph = context.graph;
    const auto& frame        = request.frame;
    const auto& view         = frame.view;
    const auto  target       = request.target;

    m_DrawState.InvalidateResources();

    m_Sampler = render_graph.Import(m_PersistentSampler, "sampler");

    const auto target_desc     = render_graph.GetResourceDesc(target);
    const auto albedo_pipeline = m_GBufferPass.GetAlbedoPipeline();

    const auto gbuffer = m_GBufferPass.CreateTargets(
        context,
        passes::GBuffer::Desc{
            .width  = target_desc.width,
            .height = target_desc.height,
        });

    for (const auto& item : frame.draw_items) {
        RecordInstance(render_graph, item);
    }
    UpdateConstantBuffer(render_graph, albedo_pipeline);

    FrameConstant frame_constant{};
    {
        frame_constant.camera_pos     = {view.camera_position, 1.0f},
        frame_constant.view           = view.view;
        frame_constant.projection     = view.projection;
        frame_constant.proj_view      = frame_constant.projection * frame_constant.view,
        frame_constant.inv_view       = math::inverse(frame_constant.view);
        frame_constant.inv_projection = math::inverse(frame_constant.projection);
        frame_constant.inv_proj_view  = math::inverse(frame_constant.proj_view);

        frame_constant.ambient_intensity = 0.62f;
        frame_constant.exposure          = 1.05f;
        frame_constant.ssao_strength     = 0.55f;
        frame_constant.viewport          = {
            static_cast<float>(target_desc.width),
            static_cast<float>(target_desc.height),
            1.0f / static_cast<float>(target_desc.width),
            1.0f / static_cast<float>(target_desc.height),
        };

        auto add_light = [&](math::vec3f world_position, math::vec3f color, float intensity) {
            if (frame_constant.light_count >= MaxDeferredLights) return;

            const auto light_index      = frame_constant.light_count++;
            const auto position         = math::vec4f(world_position, 1.0f);
            const auto position_in_view = frame_constant.view * position;

            frame_constant.lights[light_index] = {
                .position_in_view = position_in_view,
                .color_intensity  = {color, intensity},
            };

            if (light_index == 0) {
                frame_constant.light_position    = position;
                frame_constant.light_pos_in_view = position_in_view;
                frame_constant.light_color       = color;
                frame_constant.light_intensity   = intensity;
            }
        };

        for (const auto& light : frame.lights) {
            add_light(light.position, light.color.rgb, std::max(light.intensity, 1.0f) * 0.18f);
        }

        if (frame_constant.light_count == 0) {
            add_light(view.camera_position + math::vec3f{0.0f, -2.0f, 1.0f}, math::Color::White().rgb, 8.0f);
            add_light(view.camera_position + math::vec3f{3.0f, -1.5f, 0.5f}, math::Color{1.0f, 0.78f, 0.55f, 1.0f}.rgb, 4.0f);
            add_light(view.camera_position + math::vec3f{-3.0f, -1.5f, 0.5f}, math::Color{0.55f, 0.72f, 1.0f, 1.0f}.rgb, 4.0f);
        }
    };

    m_GBufferPass.BuildAlbedoPass(
        context,
        m_DrawState,
        passes::GBuffer::AlbedoPassDesc{
            .pass_name                = std::pmr::string(std::format("DeferredGBufferAlbedoPass-{}", render_graph.GetFrameIndex())),
            .target                   = gbuffer.albedo,
            .depth                    = gbuffer.depth,
            .frame_constant           = frame_constant,
            .frame_constant_buffer    = m_FrameConstantBuffer,
            .instance_constant_buffer = m_InstanceConstantBuffer,
            .bindless_info_buffer     = m_BindlessInfoConstantBuffer,
            .sampler                  = m_Sampler,
            .device_type              = m_GfxDevice.device_type,
            .width                    = target_desc.width,
            .height                   = target_desc.height,
            .instance_stride = m_InstanceConstantStride,
            .bindless_stride = m_DrawBindlessInfoStride,
        });

    const auto  normal_pipeline = m_GBufferPass.GetNormalPipeline();
    std::size_t num_draws       = 0;
    for (const auto& instance_info : m_DrawState.instance_infos) {
        num_draws += instance_info.mesh->sub_meshes.size();
    }

    m_NormalBindlessInfoConstantBuffer = render_graph.Create({
        .name   = "normal_bindless_infos",
        .size   = m_DrawBindlessInfoStride * std::max<std::size_t>(1, num_draws),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });

    // The albedo pass uploads per-frame, instance, and material data;
    // dependent G-buffer passes read its output to stay in a later recording layer.
    m_GBufferPass.BuildAttributePass(
        context,
        m_DrawState,
        passes::GBuffer::AttributePassDesc{
            .pass_name         = std::pmr::string(std::format("DeferredGBufferNormalPass-{}", render_graph.GetFrameIndex())),
            .target            = gbuffer.normal,
            .depth             = gbuffer.depth,
            .dependency        = gbuffer.albedo,
            .frame_constant    = m_FrameConstantBuffer,
            .instance_constant = m_InstanceConstantBuffer,
            .bindless_info     = m_NormalBindlessInfoConstantBuffer,
            .sampler           = m_Sampler,
            .pipeline          = normal_pipeline,
            .width             = target_desc.width,
            .height            = target_desc.height,
            .instance_stride = m_InstanceConstantStride,
            .bindless_stride = m_DrawBindlessInfoStride,
        });

    const auto material_pipeline = m_GBufferPass.GetMaterialPipeline();

    m_MaterialBindlessInfoConstantBuffer = render_graph.Create({
        .name   = "material_bindless_infos",
        .size   = m_DrawBindlessInfoStride * std::max<std::size_t>(1, num_draws),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });

    m_GBufferPass.BuildAttributePass(
        context,
        m_DrawState,
        passes::GBuffer::AttributePassDesc{
            .pass_name         = std::pmr::string(std::format("DeferredGBufferMaterialPass-{}", render_graph.GetFrameIndex())),
            .target            = gbuffer.material,
            .depth             = gbuffer.depth,
            .dependency        = gbuffer.normal,
            .frame_constant    = m_FrameConstantBuffer,
            .instance_constant = m_InstanceConstantBuffer,
            .bindless_info     = m_MaterialBindlessInfoConstantBuffer,
            .sampler           = m_Sampler,
            .pipeline          = material_pipeline,
            .width             = target_desc.width,
            .height            = target_desc.height,
            .instance_stride = m_InstanceConstantStride,
            .bindless_stride = m_DrawBindlessInfoStride,
        });

    const auto emissive_pipeline = m_GBufferPass.GetEmissivePipeline();

    m_EmissiveBindlessInfoConstantBuffer = render_graph.Create({
        .name   = "emissive_bindless_infos",
        .size   = m_DrawBindlessInfoStride * std::max<std::size_t>(1, num_draws),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });

    m_GBufferPass.BuildAttributePass(
        context,
        m_DrawState,
        passes::GBuffer::AttributePassDesc{
            .pass_name         = std::pmr::string(std::format("DeferredGBufferEmissivePass-{}", render_graph.GetFrameIndex())),
            .target            = gbuffer.emissive,
            .depth             = gbuffer.depth,
            .dependency        = gbuffer.albedo,
            .frame_constant    = m_FrameConstantBuffer,
            .instance_constant = m_InstanceConstantBuffer,
            .bindless_info     = m_EmissiveBindlessInfoConstantBuffer,
            .sampler           = m_Sampler,
            .pipeline          = emissive_pipeline,
            .width             = target_desc.width,
            .height            = target_desc.height,
            .instance_stride = m_InstanceConstantStride,
            .bindless_stride = m_DrawBindlessInfoStride,
        });

    DeferredRenderResources resources{
        .color            = target,
        .depth            = gbuffer.depth,
        .linear_depth     = gbuffer.albedo,
        .object_id        = gbuffer.object_material_id,
        .material_id      = gbuffer.object_material_id,
        .gbuffer_albedo   = gbuffer.albedo,
        .gbuffer_normal   = gbuffer.normal,
        .gbuffer_material = gbuffer.material,
        .gbuffer_emissive = gbuffer.emissive,
        .frame_constant   = m_FrameConstantBuffer,
        .sampler          = m_Sampler,
        .width            = target_desc.width,
        .height           = target_desc.height,
    };
    const DeferredDrawData draw_data{
        .instances  = m_DrawState.instance_infos,
        .draw_state = &m_DrawState,
    };
    for (const auto& extension : m_Extensions) {
        extension->AfterGBuffer(context, view, resources, draw_data);
    }

    if (request.debug_view != RenderGraphDebugView::Final) {
        m_GBufferDebugViewPass.Build(
            context,
            gbuffer,
            m_Sampler,
            RenderGraphDebugViewName(request.debug_view),
            target);
        return RenderResult{
            .color        = target,
            .depth        = resources.depth,
            .linear_depth = resources.linear_depth,
            .normal       = resources.gbuffer_normal,
        };
    }

    const auto lighting_pipeline = m_DeferredLightingPass.GetPipeline(target_desc.format);

    m_DeferredLightingBindlessInfoConstantBuffer = render_graph.Create({
        .name   = "deferred_lighting_bindless_info",
        .size   = utils::align(sizeof(passes::DeferredLighting::BindlessInfo), gfx::GPUBuffer::GetStorageViewRequirements(render_graph.GetDevice()).size_alignment),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });

    resources.color = m_DeferredLightingPass.Build(
        context,
        gbuffer,
        m_FrameConstantBuffer,
        m_DeferredLightingBindlessInfoConstantBuffer,
        m_Sampler,
        lighting_pipeline,
        target);

    for (const auto& extension : m_Extensions) {
        extension->AfterLighting(context, view, resources, draw_data);
    }

    return RenderResult{
        .color        = resources.color,
        .depth        = resources.depth,
        .linear_depth = resources.linear_depth,
        .normal       = resources.gbuffer_normal,
    };
}

void DeferredRenderer::RecordMaterial(rg::RenderGraph&, const std::shared_ptr<asset::Material>& material) {
    if (!material) return;

    material->Load(m_LoadContext);
    const auto* gbuffer_pass = FindMaterialPassForRenderPass(*material, MaterialPass::GBuffer);
    if (!gbuffer_pass) return;

    m_DrawState.material_texture_infos.try_emplace(
        material.get(),
        MaterialTextureInfo{
            .material               = material,
            .material_pass_contract = gbuffer_pass->pass_contract,
        });

    m_DrawState.active_materials.emplace(material.get());
}

void DeferredRenderer::RecordMesh(rg::RenderGraph& render_graph, const std::shared_ptr<asset::Mesh>& mesh) {
    if (!mesh || mesh->Empty()) return;

    auto [it, inserted] = m_DrawState.mesh_infos.try_emplace(mesh.get(), MeshInfo{.mesh = mesh});
    auto& info          = it->second;

    if (inserted) {
        mesh->Load(m_LoadContext);

        magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
            auto attr_data = mesh->vertices->GetAttributeData(attr);
            if (!attr_data.has_value()) return;
            info.vertices[attr] = render_graph.Import(attr_data->get().gpu_buffer);
        });

        info.indices = render_graph.Import(mesh->indices->GetGPUData());
        info.index_format = ToIndexFormat(mesh->indices->Type());
    }

    for (const auto& sub_mesh : mesh->sub_meshes) {
        RecordMaterial(render_graph, sub_mesh.material);
    }
}

void DeferredRenderer::RecordInstance(rg::RenderGraph& render_graph, const RenderDrawItem& item) {
    if (!item.mesh || item.mesh->Empty()) return;

    RecordMesh(render_graph, item.mesh);
    m_DrawState.instance_infos.emplace_back(InstanceInfo{
        .mesh          = item.mesh,
        .instance_data = {
            .model = item.transform,
        },
        .instance_index = m_DrawState.instance_infos.size(),
        .object_id      = item.object_id,
    });
}

void DeferredRenderer::UpdateConstantBuffer(rg::RenderGraph& render_graph, const std::shared_ptr<gfx::RenderPipeline>& default_pipeline) {
    for (auto* const active_material : m_DrawState.active_materials) {
        const auto material = m_DrawState.material_texture_infos.at(active_material).material;
        if (!material) continue;

        const auto& material_texture_info = m_DrawState.material_texture_infos.at(active_material);
        const auto* material_pass         = material->FindPass(material_texture_info.material_pass_contract);
        if (!material_pass) continue;

        auto pipeline = default_pipeline;
        if (material_pass->pipeline) {
            material_pass->pipeline->Load(m_LoadContext);
            if (auto built_pipeline = material_pass->pipeline->GetBuiltPipeline()) {
                pipeline = std::move(built_pipeline);
            }
        }
        if (!pipeline) continue;

        auto material_data_handle = render_graph.Create(
            {
                .name   = std::pmr::string(material->GetName()),
                .size   = utils::align(material_pass->material_data.GetDataSize(), gfx::GPUBuffer::GetStorageViewRequirements(render_graph.GetDevice()).size_alignment),
                .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
            },
            material->GetName());

        m_DrawState.material_infos.emplace(
            material.get(),
            MaterialInfo{
                .material               = material,
                .material_pass_contract = material_texture_info.material_pass_contract,
                .pipeline               = std::move(pipeline),
                .material_data          = material_data_handle,
                .pass_participation     = GetMaterialPassParticipation(*material),
            });
    }

    m_InstanceConstantStride = sizeof(InstanceConstant);
    m_DrawBindlessInfoStride = sizeof(DrawBindlessInfo);
    m_InstanceConstantBuffer = render_graph.Create({
        .name   = "instance_constant",
        .size   = m_InstanceConstantStride * std::max<std::size_t>(1, m_DrawState.instance_infos.size()),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });

    m_FrameConstantBuffer = render_graph.Create(
        {
            .name   = "frame_constant",
            .size   = utils::align(sizeof(FrameConstant), gfx::GPUBuffer::GetStorageViewRequirements(render_graph.GetDevice()).size_alignment),
            .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
        });

    std::size_t num_draws = 0;
    for (const auto& instance_info : m_DrawState.instance_infos) {
        num_draws += instance_info.mesh->sub_meshes.size();
    }

    m_BindlessInfoConstantBuffer = render_graph.Create({
        .name   = "bindless_infos",
        .size   = m_DrawBindlessInfoStride * std::max<std::size_t>(1, num_draws),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });
}
void DeferredRenderer::ClearFrameState() {
    m_Sampler                                    = {};
    m_FrameConstantBuffer                        = {};
    m_InstanceConstantBuffer                     = {};
    m_BindlessInfoConstantBuffer                 = {};
    m_NormalBindlessInfoConstantBuffer           = {};
    m_MaterialBindlessInfoConstantBuffer         = {};
    m_EmissiveBindlessInfoConstantBuffer         = {};
    m_DeferredLightingBindlessInfoConstantBuffer = {};
    m_GBufferDebugViewBindlessInfoConstantBuffer = {};

    m_DrawState.ClearFrame();
}

}  // namespace hitagi::render
