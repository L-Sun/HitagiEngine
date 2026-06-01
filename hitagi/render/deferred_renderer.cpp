module;

#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

#include <cstdlib>

#undef near
#undef far

module render;
import magic_enum;
import std;

namespace hitagi::render {

auto DeferredRenderer::Render(RenderContext& context, const RenderRequest& request) -> RenderResult {
    if (request.view.scene == nullptr || request.view.camera == nullptr) {
        return RenderResult{.color = request.target};
    }
    return RenderScene(context, request);
}

void DeferredRenderer::AddExtension(std::shared_ptr<IDeferredRenderExtension> extension) {
    if (extension) m_Extensions.emplace_back(std::move(extension));
}

void DeferredRenderer::ClearExtensions() {
    m_Extensions.clear();
}

DeferredRenderer::DeferredRenderer(gfx::Device& device, const Application& app, std::string_view name)
    : IRenderer(std::format("DeferredRenderer{}", name.empty() ? "" : std::format("({})", name))),
      m_App(app),
      m_GfxDevice(device),
      m_PersistentSampler(m_GfxDevice.CreateSampler({
          .name          = "sampler",
          .address_u     = gfx::AddressMode::Repeat,
          .address_v     = gfx::AddressMode::Repeat,
          .address_w     = gfx::AddressMode::Repeat,
          .mag_filter    = gfx::FilterMode::Linear,
          .min_filter    = gfx::FilterMode::Linear,
          .mipmap_filter = gfx::FilterMode::Linear,
      })),
      m_GBufferPass(m_GfxDevice, m_App.GetConfig().asset_root_path / "shaders" / "deferred_gbuffer.hlsl"),
      m_DeferredLightingPass(m_GfxDevice, m_App.GetConfig().asset_root_path / "shaders" / "deferred_lighting.hlsl"),
      m_GBufferDebugViewPass(m_GfxDevice, m_App.GetConfig().asset_root_path / "shaders" / "deferred_debug.hlsl") {}

auto DeferredRenderer::RenderScene(RenderContext& context, const RenderRequest& request) -> RenderResult {
    ZoneScoped;
    auto& render_graph = context.graph;
    const auto& view   = request.view;
    const auto  target = request.target;
    auto  scene        = view.scene;
    const auto& camera = *view.camera;
    const auto camera_transform = view.camera_transform;

    if (scene.get() != m_CachedScene) {
        render_graph.ClearImportedResources();
        m_CachedScene = scene.get();
    }
    m_SceneDrawState.InvalidateScene();

    m_Sampler = render_graph.Import(m_PersistentSampler, "sampler");

    const auto target_desc = render_graph.GetResourceDesc(target);
    const auto albedo_pipeline = m_GBufferPass.ImportAlbedoPipeline(context);

    const auto gbuffer = m_GBufferPass.CreateTargets(
        context,
        passes::GBuffer::Desc{
            .width  = target_desc.width,
            .height = target_desc.height,
        });

    const math::vec3f global_eye      = (camera_transform * math::vec4f(camera.parameters.eye, 1.0f)).xyz;
    const math::vec3f global_look_dir = (camera_transform * math::vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const math::vec3f global_up       = (camera_transform * math::vec4f(camera.parameters.up, 0.0f)).xyz;

    const auto view_matrix = math::look_at(global_eye, global_look_dir, global_up);
    const auto projection = math::perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip);
    const auto proj_view  = projection * view_matrix;
    const auto frustum    = math::extract_frustum(proj_view);

    for (const auto entity : scene->GetMeshEntities()) {
        const auto mesh      = entity.Get<asset::MeshComponent>().mesh;
        const auto transform = entity.Get<asset::Transform>().world_matrix;

        if (mesh->aabb.Valid()) {
            auto world_aabb = math::transform_aabb(transform, mesh->aabb);
            if (!math::is_aabb_visible(frustum, world_aabb)) continue;
        }

        RecordInstance(render_graph, entity, mesh, transform);
    }
    UpdateConstantBuffer(render_graph, albedo_pipeline);

    SceneFrameConstant frame_constant{};
    {
        const math::vec3f global_eye      = (camera_transform * math::vec4f(camera.parameters.eye, 1.0f)).xyz;
        const math::vec3f global_look_dir = (camera_transform * math::vec4f(camera.parameters.look_dir, 0.0f)).xyz;
        const math::vec3f global_up       = (camera_transform * math::vec4f(camera.parameters.up, 0.0f)).xyz;

        frame_constant.camera_pos     = {global_eye, 1.0f},
        frame_constant.view           = math::look_at(global_eye, global_look_dir, global_up);
        frame_constant.projection     = math::perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip);
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

        for (const auto entity : scene->GetLightEntities()) {
            const auto light = entity.Get<asset::LightComponent>().light;
            if (!light) continue;

            const auto light_transform = entity.Get<asset::Transform>().world_matrix;
            add_light(light_transform.col(3).xyz, light->parameters.color.rgb, std::max(light->parameters.intensity, 1.0f) * 0.18f);
        }

        if (frame_constant.light_count == 0) {
            const auto forward = math::normalize(global_look_dir);
            const auto right   = math::normalize(math::cross(forward, global_up));
            add_light(global_eye - forward * 2.0f + global_up * 1.0f, math::Color::White().rgb, 8.0f);
            add_light(global_eye - forward * 1.5f + right * 3.0f, math::Color{1.0f, 0.78f, 0.55f, 1.0f}.rgb, 4.0f);
            add_light(global_eye - forward * 1.5f - right * 3.0f, math::Color{0.55f, 0.72f, 1.0f, 1.0f}.rgb, 4.0f);
        }
    };

    m_GBufferPass.BuildAlbedoPass(
        context,
        m_SceneDrawState,
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
        });

    const auto normal_pipeline = m_GBufferPass.ImportNormalPipeline(context);
    std::size_t num_draws = 0;
    for (const auto& instance_info : m_SceneDrawState.instance_infos) {
        num_draws += instance_info.mesh->sub_meshes.size();
    }

    m_NormalBindlessInfoConstantBuffer = render_graph.Create({
        .name          = "normal_bindless_infos",
        .element_size  = sizeof(DrawBindlessInfo),
        .element_count = std::max<std::size_t>(1, num_draws),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    // The albedo pass uploads per-frame, instance, and material constants;
    // dependent G-buffer passes read its output to stay in a later recording layer.
    m_GBufferPass.BuildAttributePass(
        context,
        m_SceneDrawState,
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
        });

    const auto material_pipeline = m_GBufferPass.ImportMaterialPipeline(context);

    m_MaterialBindlessInfoConstantBuffer = render_graph.Create({
        .name          = "material_bindless_infos",
        .element_size  = sizeof(DrawBindlessInfo),
        .element_count = std::max<std::size_t>(1, num_draws),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    m_GBufferPass.BuildAttributePass(
        context,
        m_SceneDrawState,
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
        });

    const auto emissive_pipeline = m_GBufferPass.ImportEmissivePipeline(context);

    m_EmissiveBindlessInfoConstantBuffer = render_graph.Create({
        .name          = "emissive_bindless_infos",
        .element_size  = sizeof(DrawBindlessInfo),
        .element_count = std::max<std::size_t>(1, num_draws),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    m_GBufferPass.BuildAttributePass(
        context,
        m_SceneDrawState,
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
        });

    DeferredRenderResources resources{
        .color            = target,
        .depth            = gbuffer.depth,
        .linear_depth     = gbuffer.albedo,
        .gbuffer_albedo   = gbuffer.albedo,
        .gbuffer_normal   = gbuffer.normal,
        .gbuffer_material = gbuffer.material,
        .gbuffer_emissive = gbuffer.emissive,
        .frame_constant   = m_FrameConstantBuffer,
        .sampler          = m_Sampler,
        .width            = target_desc.width,
        .height           = target_desc.height,
    };
    const DeferredSceneDrawData draw_data{
        .instances        = m_SceneDrawState.instance_infos,
        .scene_draw_state = &m_SceneDrawState,
    };
    for (const auto& extension : m_Extensions) {
        extension->AfterGBuffer(context, view, resources, draw_data);
    }

    const auto debug_view = std::getenv("HITAGI_RENDER_DEBUG_VIEW");
    if (debug_view != nullptr && std::string_view(debug_view) != "final") {
        m_GBufferDebugViewPass.Build(
            context,
            gbuffer,
            m_Sampler,
            debug_view,
            target);
        return RenderResult{
            .color        = target,
            .depth        = resources.depth,
            .linear_depth = resources.linear_depth,
            .normal       = resources.gbuffer_normal,
        };
    }

    const auto lighting_pipeline = m_DeferredLightingPass.ImportPipeline(context, target_desc.format);

    m_DeferredLightingBindlessInfoConstantBuffer = render_graph.Create({
        .name          = "deferred_lighting_bindless_info",
        .element_size  = sizeof(passes::DeferredLighting::BindlessInfo),
        .element_count = 1,
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
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

void DeferredRenderer::RecordMaterialInstance(rg::RenderGraph& render_graph, const std::shared_ptr<asset::MaterialInstance>& material_instance) {
    if (!material_instance) return;
    if (!material_instance->GetMaterial()) {
        spdlog::warn("Material instance '{}' has no material assigned, skipping", material_instance->GetName());
        return;
    }
    auto [it, inserted] = m_SceneDrawState.material_instance_infos.try_emplace(
        material_instance.get(),
        MaterialInstanceInfo{
            .material_instance = material_instance,
        });
    auto& info = it->second;

    if (inserted) {
        auto associated = material_instance->GetAssociatedTextures();
        for (std::size_t ti = 0; ti < associated.size(); ti++) {
            const auto& texture = associated[ti];
            if (texture && !texture->Empty()) {
                texture->InitGPUData(m_GfxDevice);
                info.textures.emplace_back(render_graph.Import(texture->GetGPUData(), texture->GetUniqueName()));
            } else {
                info.textures.emplace_back(rg::TextureHandle{});
            }
        }
    }

    m_SceneDrawState.active_material_instances.emplace(material_instance.get());
}

void DeferredRenderer::RecordMesh(rg::RenderGraph& render_graph, const std::shared_ptr<asset::Mesh>& mesh) {
    auto [it, inserted] = m_SceneDrawState.mesh_infos.try_emplace(mesh.get(), MeshInfo{.mesh = mesh});
    auto& info          = it->second;

    if (inserted) {
        mesh->vertices->InitGPUData(m_GfxDevice);
        mesh->indices->InitGPUData(m_GfxDevice);

        magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
            auto attr_data = mesh->vertices->GetAttributeData(attr);
            if (!attr_data.has_value()) return;
            info.vertices[attr] = render_graph.Import(attr_data->get().gpu_buffer);
        });

        info.indices = render_graph.Import(mesh->indices->GetIndexData().gpu_buffer);
    }

    for (const auto& sub_mesh : mesh->sub_meshes) {
        RecordMaterialInstance(render_graph, sub_mesh.material_instance);
    }
}

void DeferredRenderer::RecordInstance(rg::RenderGraph& render_graph, ecs::Entity entity, const std::shared_ptr<asset::Mesh>& mesh, math::mat4f transform) {
    RecordMesh(render_graph, mesh);
    m_SceneDrawState.instance_infos.emplace_back(InstanceInfo{
        .entity        = entity,
        .mesh          = mesh,
        .instance_data = {
            .model = transform,
        },
        .instance_index = m_SceneDrawState.instance_infos.size(),
    });
}

void DeferredRenderer::UpdateConstantBuffer(rg::RenderGraph& render_graph, rg::RenderPipelineHandle albedo_pipeline) {
    std::pmr::unordered_map<std::shared_ptr<asset::Material>, std::size_t> material_instance_counter;
    m_SceneDrawState.material_instance_indices.clear();
    for (auto* const material_instance : m_SceneDrawState.active_material_instances) {
        const auto material = material_instance->GetMaterial();
        m_SceneDrawState.material_instance_indices.emplace(material_instance, material_instance_counter[material]++);
    }

    for (const auto& [material, num_instances] : material_instance_counter) {
        auto pipeline_it = m_SceneDrawState.pipeline_handles.find(material.get());
        if (pipeline_it == m_SceneDrawState.pipeline_handles.end()) {
            pipeline_it = m_SceneDrawState.pipeline_handles.emplace(material.get(), albedo_pipeline).first;
        }
        auto pipeline_handle = pipeline_it->second;

        auto constant_handle = render_graph.Create(
            {
                .name          = std::pmr::string(material->GetName()),
                .element_size  = material->CalculateMaterialBufferSize(m_GfxDevice.device_type == gfx::Device::Type::DX12),
                .element_count = num_instances,  // we not use material->GetNumInstances for reduce memory usage
                .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
            },
            material->GetName());

        m_SceneDrawState.material_infos.emplace(
            material.get(),
            MaterialInfo{
                .material          = material,
                .pipeline          = pipeline_handle,
                .material_constant = constant_handle,
            });
    }

    m_InstanceConstantBuffer = render_graph.Create({
        .name          = "instance_constant",
        .element_size  = sizeof(SceneInstanceConstant),
        .element_count = std::max<std::size_t>(1, m_SceneDrawState.instance_infos.size()),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    m_FrameConstantBuffer = render_graph.Create(
        {
            .name          = "frame_constant",
            .element_size  = sizeof(SceneFrameConstant),
            .element_count = 1,
            .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
        });

    std::size_t num_draws = 0;
    for (const auto& instance_info : m_SceneDrawState.instance_infos) {
        num_draws += instance_info.mesh->sub_meshes.size();
    }

    m_BindlessInfoConstantBuffer = render_graph.Create({
        .name          = "bindless_infos",
        .element_size  = sizeof(DrawBindlessInfo),
        .element_count = std::max<std::size_t>(1, num_draws),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });
}
void DeferredRenderer::ClearFrameState() {
    m_Sampler                                     = {};
    m_FrameConstantBuffer                         = {};
    m_InstanceConstantBuffer                      = {};
    m_BindlessInfoConstantBuffer                  = {};
    m_NormalBindlessInfoConstantBuffer            = {};
    m_MaterialBindlessInfoConstantBuffer          = {};
    m_EmissiveBindlessInfoConstantBuffer          = {};
    m_DeferredLightingBindlessInfoConstantBuffer = {};
    m_GBufferDebugViewBindlessInfoConstantBuffer = {};

    m_SceneDrawState.ClearFrame();
}

}  // namespace hitagi::render
