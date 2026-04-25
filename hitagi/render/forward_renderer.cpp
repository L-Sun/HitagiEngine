module;

#include <imgui.h>
#include <range/v3/all.hpp>
#include <spdlog/spdlog.h>
#include <tracy/Tracy.hpp>

#undef near
#undef far

module render;
import magic_enum;
import std;

namespace hitagi::render {

ForwardRenderer::ForwardRenderer(gfx::Device& device, const Application& app, gui::GuiManager* gui_manager, std::string_view name)
    : IRenderer(std::format("ForwardRenderer{}", name.empty() ? "" : std::format("({})", name))),
      m_App(app),
      m_GfxDevice(device),
      m_SwapChain(m_GfxDevice.CreateSwapChain({
          .name        = "SwapChain",
          .window      = app.GetWindow(),
          .clear_color = math::Color(0, 0, 0, 1),
      })),
      m_PersistentSampler(m_GfxDevice.CreateSampler({.name = "sampler"})),
      m_RenderGraph(m_GfxDevice, "ForwardRenderGraph"),
      m_GuiRenderUtils(gui_manager ? std::make_unique<GuiRenderUtils>(*gui_manager, m_GfxDevice) : nullptr),
      m_TextRenderUtils(std::make_unique<TextRenderUtils>(m_GfxDevice, m_App.GetConfig().asset_root_path / "fonts"))

{
    m_Clock.Start();
}

void ForwardRenderer::Tick() {
    ZoneScopedN("RenderFrame");

    if (m_App.WindowSizeChanged()) {
        m_SwapChain->Resize();
    }

    if (m_RenderGraph.IsValid(m_GuiTarget)) {
        m_GuiRenderUtils->GuiPass(m_RenderGraph, m_GuiTarget, m_ClearGuiTarget);
    }

    if (m_RenderGraph.Compile()) {
        m_RenderGraph.Execute();
    }

    ClearFrameState();

    m_SwapChain->Present();

    m_Clock.Tick();

    static bool tracy_plot_configured = false;
    if (!tracy_plot_configured) {
        TracyPlotConfig("Render Frame Time (ms)", tracy::PlotFormatType::Number, false, true, 0);
        tracy_plot_configured = true;
    }
    TracyPlot("Render Frame Time (ms)", m_Clock.DeltaTime().count() * 1000.0);
}

void ForwardRenderer::RenderScene(std::shared_ptr<asset::Scene> scene, const asset::Camera& camera, math::mat4f camera_transform, rg::TextureHandle target) {
    ZoneScoped;

    if (scene.get() != m_CachedScene) {
        InvalidateSceneCaches();
        m_CachedScene = scene.get();
    }

    m_Sampler = m_RenderGraph.Import(m_PersistentSampler, "sampler");

    rg::RenderPassBuilder render_pass_builder(m_RenderGraph);
    render_pass_builder
        .SetName(std::format("ColorPass-{}", m_RenderGraph.GetFrameIndex()))
        .SetRenderTarget(target, true)
        .SetDepthStencil(
            m_RenderGraph.Create(gfx::TextureDesc{
                .name        = "depth_stencil",
                .width       = m_RenderGraph.GetResourceDesc(target).width,
                .height      = m_RenderGraph.GetResourceDesc(target).height,
                .format      = gfx::Format::D32_FLOAT,
                .clear_value = gfx::ClearDepthStencil{
                    .depth   = 1.0f,
                    .stencil = 0,
                },
                .usages = gfx::TextureUsageFlags::DepthStencil,
            }),
            true)
        .AddSampler(m_Sampler);

    const math::vec3f global_eye      = (camera_transform * math::vec4f(camera.parameters.eye, 1.0f)).xyz;
    const math::vec3f global_look_dir = (camera_transform * math::vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const math::vec3f global_up       = (camera_transform * math::vec4f(camera.parameters.up, 0.0f)).xyz;

    const auto view       = math::look_at(global_eye, global_look_dir, global_up);
    const auto projection = math::perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip);
    const auto proj_view  = projection * view;
    const auto frustum    = math::extract_frustum(proj_view);

    for (const auto entity : scene->GetMeshEntities()) {
        const auto mesh      = entity.Get<asset::MeshComponent>().mesh;
        const auto transform = entity.Get<asset::Transform>().world_matrix;

        if (mesh->aabb.Valid()) {
            auto world_aabb = math::transform_aabb(transform, mesh->aabb);
            if (!math::is_aabb_visible(frustum, world_aabb)) continue;
        }

        RecordInstance(render_pass_builder, mesh, transform);
    }
    UpdateConstantBuffer(render_pass_builder);

    FrameConstant frame_constant;
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

        if (!scene->GetLightEntities().empty()) {
            const auto light           = scene->GetLightEntities().front().Get<asset::LightComponent>().light;
            const auto light_transform = scene->GetLightEntities().front().Get<asset::Transform>().world_matrix;

            frame_constant.light_position    = light_transform.col(3);
            frame_constant.light_pos_in_view = frame_constant.view * frame_constant.light_position;
            frame_constant.light_color       = light->parameters.color.rgb;
            frame_constant.light_intensity   = light->parameters.intensity;
        }
    };

    render_pass_builder.SetExecutor([this, frame_constant, target](const rg::RenderGraph& render_graph, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        // update frame constant
        gfx::GPUBufferView<FrameConstant>(pass.Resolve(m_FrameConstantBuffer)).front() = frame_constant;

        gfx::GPUBufferView<BindlessInfo> bindless_infos(pass.Resolve(m_BindlessInfoConstantBuffer));

        const auto& render_target = pass.Resolve(target);
        cmd.SetViewPort({
            .x      = 0,
            .y      = 0,
            .width  = static_cast<float>(render_target.GetDesc().width),
            .height = static_cast<float>(render_target.GetDesc().height),
        });
        cmd.SetScissorRect({
            .x      = 0,
            .y      = 0,
            .width  = render_target.GetDesc().width,
            .height = render_target.GetDesc().height,
        });

        // update material instance data
        for (auto* const material_instance : m_ActiveMaterialInstances) {
            auto       material_info            = m_MaterialInfos.at(material_instance->GetMaterial().get());
            auto&      material_constant_buffer = pass.Resolve(material_info.material_constant);
            const auto material_instance_index  = m_MaterialInstanceIndices.at(material_instance);

            auto material_constant_data = material_instance->GenerateMaterialBuffer(m_GfxDevice.device_type == gfx::Device::Type::DX12);

            std::memcpy(material_constant_buffer.Map() + material_constant_buffer.AlignedElementSize() * material_instance_index,
                        material_constant_data.GetData(),
                        material_constant_data.GetDataSize());
            material_constant_buffer.UnMap();
        }

        // update instance data
        auto instance_constant = gfx::GPUBufferView<InstanceConstant>(pass.Resolve(m_InstanceConstantBuffer));
        for (const auto& instance_info : m_InstanceInfos) {
            instance_constant[instance_info.instance_index] = instance_info.instance_data;
        }

        std::size_t draw_index = 0;
        for (const auto& instance_info : m_InstanceInfos) {
            const auto mesh_info = m_MeshInfos.at(instance_info.mesh.get());

            for (const auto& sub_mesh : instance_info.mesh->sub_meshes) {
                const auto& material_instance_info = m_MaterialInstanceInfos.at(sub_mesh.material_instance.get());
                const auto& material_info          = m_MaterialInfos.at(sub_mesh.material_instance->GetMaterial().get());

                auto& pipeline = pass.Resolve(material_info.pipeline);
                cmd.SetPipeline(pipeline);

                const auto material_constant_handle = material_info.material_constant;
                const auto material_instance_index  = m_MaterialInstanceIndices.at(sub_mesh.material_instance.get());

                bindless_infos[draw_index] = {
                    .frame_constant    = pass.GetBindless(m_FrameConstantBuffer),
                    .instance_constant = pass.GetBindless(m_InstanceConstantBuffer, instance_info.instance_index),
                    .material_constant = pass.GetBindless(material_constant_handle, material_instance_index),
                    .sampler           = pass.GetBindless(m_Sampler),
                };

                for (auto [texture_bindless, texture_handle] : ranges::views::zip(bindless_infos[draw_index].textures, material_instance_info.textures)) {
                    if (texture_handle) {
                        texture_bindless = pass.GetBindless(texture_handle);
                    }
                }

                cmd.PushBindlessMetaInfo({
                    .handle = pass.GetBindless(m_BindlessInfoConstantBuffer, draw_index),
                });
                for (const auto& vertex_attr : pipeline.GetDesc().vertex_input_layout) {
                    auto mesh_attr   = asset::semantic_to_vertex_attribute(vertex_attr.semantic);
                    auto attr_handle = mesh_info.vertices[mesh_attr];
                    if (attr_handle) {
                        cmd.SetVertexBuffers(vertex_attr.binding, {{pass.Resolve(attr_handle)}}, {{0}});
                    }
                }
                cmd.SetIndexBuffer(pass.Resolve(mesh_info.indices), 0);

                cmd.DrawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, sub_mesh.vertex_offset);

                // Command buffers read bindless_infos later on the GPU, so each draw
                // must point at a stable slot instead of reusing index 0.
                ++draw_index;
            }
        }
    });

    render_pass_builder.Finish();
}

void ForwardRenderer::RenderGui(rg::TextureHandle target, bool clear_target) {
    m_GuiTarget      = target;
    m_ClearGuiTarget = clear_target;
}

void ForwardRenderer::RenderText(rg::TextureHandle target, std::span<const TextDrawCommand> commands, bool clear_target) {
    m_TextRenderUtils->TextPass(m_RenderGraph, target, commands, clear_target);
}

void ForwardRenderer::CopyToTexture(rg::TextureHandle from, std::shared_ptr<gfx::Texture> to, gfx::TextureSubresourceLayer from_layer, gfx::TextureSubresourceLayer to_layer) {
    m_RenderGraph.QueueTextureExtraction(from, std::move(to), from_layer, to_layer);
}

void ForwardRenderer::CopyToBuffer(rg::TextureHandle from, std::shared_ptr<gfx::GPUBuffer> to, gfx::TextureSubresourceLayer from_layer) {
    m_RenderGraph.QueueBufferExtraction(from, std::move(to), from_layer);
}

void ForwardRenderer::ToSwapChain(rg::TextureHandle from) {
    rg::PresentPassBuilder(m_RenderGraph)
        .From(from)
        .SetSwapChain(m_SwapChain)
        .Finish();
}

void ForwardRenderer::RecordMaterialInstance(rg::RenderPassBuilder& builder, const std::shared_ptr<asset::MaterialInstance>& material_instance) {
    if (!material_instance) return;
    if (!material_instance->GetMaterial()) {
        spdlog::warn("Material instance '{}' has no material assigned, skipping", material_instance->GetName());
        return;
    }
    auto [it, inserted] = m_MaterialInstanceInfos.try_emplace(
        material_instance.get(),
        MaterialInstanceInfo{
            .material_instance = material_instance,
        });
    auto& info = it->second;

    if (inserted) {
        auto associated = material_instance->GetAssociatedTextures();
        if (associated.empty() && material_instance->GetMaterial()) {
            spdlog::info("  '{}' (mat='{}') has 0 associated textures", material_instance->GetName(), material_instance->GetMaterial()->GetName());
        }
        for (std::size_t ti = 0; ti < associated.size(); ti++) {
            const auto& texture = associated[ti];
            if (texture && !texture->Empty()) {
                texture->InitGPUData(m_GfxDevice);
                info.textures.emplace_back(m_RenderGraph.Import(texture->GetGPUData(), texture->GetUniqueName()));
            } else {
                if (m_InstanceInfos.size() <= 1) {
                    spdlog::info("  '{}' texture[{}] = {}", material_instance->GetName(), ti, texture ? "empty" : "null");
                }
                info.textures.emplace_back(rg::TextureHandle{});
            }
        }
    }

    for (const auto texture : info.textures) {
        if (texture) {
            builder.Read(texture, {}, gfx::PipelineStage::PixelShader);
        }
    }
    m_ActiveMaterialInstances.emplace(material_instance.get());
}

void ForwardRenderer::RecordMesh(rg::RenderPassBuilder& builder, const std::shared_ptr<asset::Mesh>& mesh) {
    auto [it, inserted] = m_MeshInfos.try_emplace(mesh.get(), MeshInfo{.mesh = mesh});
    auto& info          = it->second;

    if (inserted) {
        mesh->vertices->InitGPUData(m_GfxDevice);
        mesh->indices->InitGPUData(m_GfxDevice);

        magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
            auto attr_data = mesh->vertices->GetAttributeData(attr);
            if (!attr_data.has_value()) return;
            info.vertices[attr] = m_RenderGraph.Import(attr_data->get().gpu_buffer);
        });

        info.indices = m_RenderGraph.Import(mesh->indices->GetIndexData().gpu_buffer);
    }

    magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
        const auto handle = info.vertices[attr];
        if (handle) {
            builder.ReadAsVertices(handle);
        }
    });
    builder.ReadAsIndices(info.indices);

    for (const auto& sub_mesh : mesh->sub_meshes) {
        RecordMaterialInstance(builder, sub_mesh.material_instance);
    }
}

void ForwardRenderer::RecordInstance(rg::RenderPassBuilder& builder, const std::shared_ptr<asset::Mesh>& mesh, math::mat4f transform) {
    RecordMesh(builder, mesh);
    m_InstanceInfos.emplace_back(InstanceInfo{
        .mesh          = mesh,
        .instance_data = {
            .model = transform,
        },
        .instance_index = m_InstanceInfos.size(),
    });
}

void ForwardRenderer::UpdateConstantBuffer(rg::RenderPassBuilder& builder) {
    std::pmr::unordered_map<std::shared_ptr<asset::Material>, std::size_t> material_instance_counter;
    m_MaterialInstanceIndices.clear();
    for (auto* const material_instance : m_ActiveMaterialInstances) {
        const auto material = material_instance->GetMaterial();
        m_MaterialInstanceIndices.emplace(material_instance, material_instance_counter[material]++);
    }

    for (const auto& [material, num_instances] : material_instance_counter) {
        auto pipeline_it = m_PipelineHandles.find(material.get());
        if (pipeline_it == m_PipelineHandles.end()) {
            pipeline_it = m_PipelineHandles.emplace(material.get(), m_RenderGraph.Import(material->GetPipeline(m_GfxDevice), material->GetName())).first;
        }
        auto pipeline_handle = pipeline_it->second;
        builder.AddPipeline(pipeline_handle);

        auto constant_handle = m_RenderGraph.Create(
            {
                .name          = std::pmr::string(material->GetName()),
                .element_size  = material->CalculateMaterialBufferSize(m_GfxDevice.device_type == gfx::Device::Type::DX12),
                .element_count = num_instances,  // we not use material->GetNumInstances for reduce memory usage
                .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
            },
            material->GetName());
        builder.Read(constant_handle);

        m_MaterialInfos.emplace(
            material.get(),
            MaterialInfo{
                .material          = material,
                .pipeline          = pipeline_handle,
                .material_constant = constant_handle,
            });
    }

    m_InstanceConstantBuffer = m_RenderGraph.Create({
        .name          = "instance_constant",
        .element_size  = sizeof(InstanceConstant),
        .element_count = std::max<std::size_t>(1, m_InstanceInfos.size()),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });
    builder.Read(m_InstanceConstantBuffer);

    m_FrameConstantBuffer = m_RenderGraph.Create(
        {
            .name          = "frame_constant",
            .element_size  = sizeof(FrameConstant),
            .element_count = 1,
            .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
        });
    builder.Read(m_FrameConstantBuffer);

    std::size_t num_draws = 0;
    for (const auto& instance_info : m_InstanceInfos) {
        num_draws += instance_info.mesh->sub_meshes.size();
    }

    m_BindlessInfoConstantBuffer = m_RenderGraph.Create({
        .name          = "bindless_infos",
        .element_size  = sizeof(BindlessInfo),
        .element_count = std::max<std::size_t>(1, num_draws),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });
    builder.Read(m_BindlessInfoConstantBuffer);
}
void ForwardRenderer::ClearFrameState() {
    m_Sampler                    = {};
    m_FrameConstantBuffer        = {};
    m_InstanceConstantBuffer     = {};
    m_BindlessInfoConstantBuffer = {};

    m_MaterialInfos.clear();
    m_MaterialInstanceIndices.clear();
    m_ActiveMaterialInstances.clear();
    m_InstanceInfos.clear();
}

void ForwardRenderer::InvalidateSceneCaches() {
    m_RenderGraph.ClearImportedResources();
    m_PipelineHandles.clear();
    m_MaterialInstanceInfos.clear();
    m_MaterialInstanceIndices.clear();
    m_ActiveMaterialInstances.clear();
    m_MeshInfos.clear();
    m_CachedScene = nullptr;
}

}  // namespace hitagi::render
