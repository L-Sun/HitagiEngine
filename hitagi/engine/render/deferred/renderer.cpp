module;

#include "interop/tracy_macros.hpp"

#undef near
#undef far

export module render:deferred_renderer;
import interop.tracy;
import interop.magic_enum;

import ecs;
import std;
import :execution;
import utils;
import math;
import core;
import gfx;
import asset;

import :deferred_types;
import :deferred_gbuffer;
import :deferred_lighting;
import :deferred_gbuffer_debug;

export namespace hitagi::render::deferred {

class DeferredRenderer final {
public:
    // Shader sources are read through `file_io` during construction only; the
    // renderer keeps no reference to it afterwards.
    DeferredRenderer(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, core::IOService& file_io, const std::filesystem::path& shader_dir);

    // Frame callbacks borrow this renderer's draw state. Execute the graph
    // before the next Build() on this instance, and keep it alive.
    auto Build(rg::RenderGraph& graph, const RenderRequest& request) -> RenderResult;
    // Copies frame arrays; the returned builder borrows this renderer.
    auto Capture(const RenderFrame& frame, RenderGraphDebugView debug_view = RenderGraphDebugView::Final) -> FrameBuilder;
    ~DeferredRenderer() = default;

    void AddExtension(std::shared_ptr<IDeferredRenderExtension> extension);
    void ClearExtensions();

private:
    auto BuildFrame(rg::RenderGraph& graph, const RenderRequest& request) -> RenderResult;
    void RecordMaterial(rg::RenderGraph& render_graph, const std::shared_ptr<asset::Material>& material);
    void RecordMesh(rg::RenderGraph& render_graph, const RenderDrawItem& item);
    void RecordInstance(rg::RenderGraph& render_graph, const RenderDrawItem& item);
    // this function must invoke after all instance are finished, it will:
    // 1. update index of material in the constant buffer of material,
    // 2. create constant buffer of materials
    // 3. create constant buffer of instances
    // 4. create constant buffer of frame
    // 5. create constant buffer of bindless info
    void UpdateConstantBuffer(rg::RenderGraph& render_graph, const std::shared_ptr<gfx::RenderPipeline>& default_pipeline);

    gfx::Device& m_GfxDevice;

    // Execution environment handed to every asset Load() call in this renderer.
    asset::ResourceLoadContext m_LoadContext;

    std::shared_ptr<gfx::Sampler> m_PersistentSampler;

    passes::GBuffer                                             m_GBufferPass;
    passes::DeferredLighting                                    m_DeferredLightingPass;
    passes::GBufferDebugView                                    m_GBufferDebugViewPass;
    std::pmr::vector<std::shared_ptr<IDeferredRenderExtension>> m_Extensions;

    // frame state
    rg::SamplerHandle   m_Sampler;
    rg::GPUBufferHandle m_FrameConstantBuffer;
    rg::GPUBufferHandle m_InstanceConstantBuffer;
    std::uint64_t       m_InstanceConstantStride = 0;
    std::uint64_t       m_DrawBindlessInfoStride = 0;
    rg::GPUBufferHandle m_BindlessInfoConstantBuffer;
    rg::GPUBufferHandle m_NormalBindlessInfoConstantBuffer;
    rg::GPUBufferHandle m_MaterialBindlessInfoConstantBuffer;
    rg::GPUBufferHandle m_EmissiveBindlessInfoConstantBuffer;
    rg::GPUBufferHandle m_DeferredLightingBindlessInfoConstantBuffer;
    rg::GPUBufferHandle m_GBufferDebugViewBindlessInfoConstantBuffer;

    RenderDrawState m_DrawState;
};

}  // namespace hitagi::render::deferred

namespace hitagi::render::deferred {

auto DeferredRenderer::Capture(const RenderFrame& frame, RenderGraphDebugView debug_view) -> FrameBuilder {
    return [this, view = frame.view, draw_items = std::pmr::vector<RenderDrawItem>(frame.draw_items.begin(), frame.draw_items.end()), lights = std::pmr::vector<RenderLight>(frame.lights.begin(), frame.lights.end()), debug_view](rg::RenderGraph& graph, rg::TextureHandle target) {
        return Build(graph, {.frame = {.view = view, .draw_items = draw_items, .lights = lights}, .target = target, .debug_view = debug_view}).color;
    };
}

constexpr auto ToIndexFormat(asset::IndexType type) noexcept -> gfx::Format {
    return type == asset::IndexType::UINT16 ? gfx::Format::R16_UINT : gfx::Format::R32_UINT;
}

auto DeferredRenderer::Build(rg::RenderGraph& graph, const RenderRequest& request) -> RenderResult {
    if (!graph.IsValid(request.target)) {
        return RenderResult{.color = request.target};
    }
    return BuildFrame(graph, request);
}

void DeferredRenderer::AddExtension(std::shared_ptr<IDeferredRenderExtension> extension) {
    if (extension) m_Extensions.emplace_back(std::move(extension));
}

void DeferredRenderer::ClearExtensions() {
    m_Extensions.clear();
}

DeferredRenderer::DeferredRenderer(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings, const gfx::ShaderCompiler& compiler, core::IOService& file_io, const std::filesystem::path& shader_dir)
    : m_GfxDevice(device),
      m_LoadContext{.device = device, .queues = queues, .bindings = bindings, .shader_compiler = compiler},
      m_PersistentSampler(hitagi::gfx::Sampler::Create(
          m_GfxDevice,
          bindings,
          {
              .name          = "sampler",
              .address_u     = gfx::AddressMode::Repeat,
              .address_v     = gfx::AddressMode::Repeat,
              .address_w     = gfx::AddressMode::Repeat,
              .mag_filter    = gfx::FilterMode::Linear,
              .min_filter    = gfx::FilterMode::Linear,
              .mipmap_filter = gfx::FilterMode::Linear,
          })),
      m_GBufferPass(m_GfxDevice, bindings, compiler, LoadShaderSource(file_io, shader_dir / "deferred_gbuffer.slang")),
      m_DeferredLightingPass(m_GfxDevice, bindings, compiler, LoadShaderSource(file_io, shader_dir / "deferred_lighting.slang")),
      m_GBufferDebugViewPass(m_GfxDevice, bindings, compiler, LoadShaderSource(file_io, shader_dir / "deferred_debug.slang")) {}

auto DeferredRenderer::BuildFrame(rg::RenderGraph& graph, const RenderRequest& request) -> RenderResult {
    ZoneScoped;
    auto&       render_graph = graph;
    const auto& frame        = request.frame;
    const auto& view         = frame.view;
    const auto  target       = request.target;

    m_DrawState.InvalidateResources();

    m_Sampler = render_graph.Import(m_PersistentSampler, "sampler");

    const auto target_desc     = render_graph.GetResourceDesc(target);
    const auto albedo_pipeline = m_GBufferPass.GetAlbedoPipeline();

    const auto gbuffer = m_GBufferPass.CreateTargets(
        graph,
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
        graph,
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
            .instance_stride          = m_InstanceConstantStride,
            .bindless_stride          = m_DrawBindlessInfoStride,
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
        graph,
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
            .instance_stride   = m_InstanceConstantStride,
            .bindless_stride   = m_DrawBindlessInfoStride,
        });

    const auto material_pipeline = m_GBufferPass.GetMaterialPipeline();

    m_MaterialBindlessInfoConstantBuffer = render_graph.Create({
        .name   = "material_bindless_infos",
        .size   = m_DrawBindlessInfoStride * std::max<std::size_t>(1, num_draws),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });

    m_GBufferPass.BuildAttributePass(
        graph,
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
            .instance_stride   = m_InstanceConstantStride,
            .bindless_stride   = m_DrawBindlessInfoStride,
        });

    const auto emissive_pipeline = m_GBufferPass.GetEmissivePipeline();

    m_EmissiveBindlessInfoConstantBuffer = render_graph.Create({
        .name   = "emissive_bindless_infos",
        .size   = m_DrawBindlessInfoStride * std::max<std::size_t>(1, num_draws),
        .usages = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::StorageRead,
    });

    m_GBufferPass.BuildAttributePass(
        graph,
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
            .instance_stride   = m_InstanceConstantStride,
            .bindless_stride   = m_DrawBindlessInfoStride,
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
        extension->AfterGBuffer(graph, view, resources, draw_data);
    }

    if (request.debug_view != RenderGraphDebugView::Final) {
        m_GBufferDebugViewPass.Build(
            graph,
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
        graph,
        gbuffer,
        m_FrameConstantBuffer,
        m_DeferredLightingBindlessInfoConstantBuffer,
        m_Sampler,
        lighting_pipeline,
        target);

    for (const auto& extension : m_Extensions) {
        extension->AfterLighting(graph, view, resources, draw_data);
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

void DeferredRenderer::RecordMesh(rg::RenderGraph& render_graph, const RenderDrawItem& item) {
    const auto& mesh = item.mesh;
    if (!mesh || mesh->Empty()) return;

    auto [it, inserted] = m_DrawState.mesh_infos.try_emplace(
        std::pair{mesh.get(), item.deformed_vertices.get()},
        MeshInfo{.mesh = mesh, .deformed_vertices = item.deformed_vertices});
    auto& info = it->second;

    if (inserted) {
        mesh->Load(m_LoadContext);
        if (item.deformed_vertices) {
            if (item.deformed_vertices->Size() != mesh->vertices->Size()) throw std::invalid_argument("Deformed vertex count mismatch");
            item.deformed_vertices->Load(m_LoadContext);
        }

        magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
            auto attr_data = mesh->vertices->GetAttributeData(attr);
            if (item.deformed_vertices) {
                if (auto deformed = item.deformed_vertices->GetAttributeData(attr)) attr_data = deformed;
            }
            if (!attr_data.has_value()) return;
            info.vertices[attr] = render_graph.Import(attr_data->get().gpu_buffer);
        });

        info.indices      = render_graph.Import(mesh->indices->GetGPUData());
        info.index_format = ToIndexFormat(mesh->indices->Type());
    }

    for (const auto& sub_mesh : item.SubMeshes()) {
        RecordMaterial(render_graph, sub_mesh.material);
    }
}

void DeferredRenderer::RecordInstance(rg::RenderGraph& render_graph, const RenderDrawItem& item) {
    if (!item.mesh || item.mesh->Empty()) return;

    RecordMesh(render_graph, item);
    m_DrawState.instance_infos.emplace_back(InstanceInfo{
        .material_overrides = item.material_overrides,
        .mesh               = item.mesh,
        .deformed_vertices  = item.deformed_vertices,
        .instance_data      = {
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

}  // namespace hitagi::render::deferred
