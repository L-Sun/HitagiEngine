#include <hitagi/render/forward_renderer.hpp>
#include <hitagi/core/thread_manager.hpp>
#include <hitagi/application.hpp>
#include <hitagi/gfx/utils.hpp>
#include <hitagi/asset/transform.hpp>

#include <imgui.h>
#include <magic_enum_utility.hpp>
#include <range/v3/all.hpp>
#include <spdlog/logger.h>
#include <tracy/Tracy.hpp>

#undef near
#undef far

namespace hitagi::render {

ForwardRenderer::ForwardRenderer(gfx::Device& device, const Application& app, gui::GuiManager* gui_manager, std::string_view name)
    : IRenderer(fmt::format("ForwardRenderer{}", name.empty() ? "" : fmt::format("({})", name))),
      m_App(app),
      m_GfxDevice(device),
      m_SwapChain(m_GfxDevice.CreateSwapChain({
          .name        = "SwapChain",
          .window      = app.GetWindow(),
          .clear_color = math::Color(0, 0, 0, 1),
      })),
      m_RenderGraph(m_GfxDevice, "ForwardRenderGraph"),
      m_GuiRenderUtils(gui_manager ? std::make_unique<GuiRenderUtils>(*gui_manager, m_GfxDevice) : nullptr)

{
    m_Clock.Start();
}

void ForwardRenderer::Tick() {
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
}

void ForwardRenderer::RenderScene(std::shared_ptr<asset::Scene> scene, const asset::Camera& camera, math::mat4f camera_transform, rg::TextureHandle target) {
    ZoneScoped;

    m_Sampler = m_RenderGraph.Create(gfx::SamplerDesc{
        .name = "sampler",
    });

    rg::RenderPassBuilder render_pass_builder(m_RenderGraph);
    render_pass_builder
        .SetName(fmt::format("ColorPass-{}", m_RenderGraph.GetFrameIndex()))
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

    for (const auto entity : scene->GetMeshEntities()) {
        const auto mesh      = entity.Get<asset::MeshComponent>().mesh;
        const auto transform = entity.Get<asset::Transform>().world_matrix;
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
        cmd.SetViewPort({0, 0, static_cast<float>(render_target.GetDesc().width), static_cast<float>(render_target.GetDesc().height)});
        cmd.SetScissorRect({0, 0, render_target.GetDesc().width, render_target.GetDesc().height});

        // update material instance data
        for (const auto& [material_instance, info] : m_MaterialInstanceInfos) {
            auto  material_info            = m_MaterialInfos.at(material_instance->GetMaterial().get());
            auto& material_constant_buffer = pass.Resolve(material_info.material_constant);

            auto material_constant_data = material_instance->GenerateMaterialBuffer(m_GfxDevice.device_type == gfx::Device::Type::DX12);

            std::memcpy(material_constant_buffer.Map() + material_constant_buffer.AlignedElementSize() * info.material_instance_index,
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
                const auto material_instance_index  = material_instance_info.material_instance_index;

                bindless_infos[draw_index] = {
                    .frame_constant    = pass.GetBindless(m_FrameConstantBuffer),
                    .instance_constant = pass.GetBindless(m_InstanceConstantBuffer, instance_info.instance_index),
                    .material_constant = pass.GetBindless(material_constant_handle, material_instance_index),
                    .sampler           = pass.GetBindless(material_instance_info.samplers[0]),
                };

                for (auto [texture_bindless, texture_handle] : ranges::views::zip(bindless_infos[draw_index].textures, material_instance_info.textures)) {
                    texture_bindless = pass.GetBindless(texture_handle);
                }

                cmd.PushBindlessMetaInfo({
                    .handle = pass.GetBindless(m_BindlessInfoConstantBuffer, draw_index),
                });
                for (const auto& vertex_attr : pipeline.GetDesc().vertex_input_layout) {
                    cmd.SetVertexBuffers(vertex_attr.binding, {{pass.Resolve(mesh_info.vertices.at(vertex_attr.binding))}}, {{0}});
                }
                cmd.SetIndexBuffer(pass.Resolve(mesh_info.indices), 0);

                cmd.DrawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, sub_mesh.vertex_offset);
            }
        }
    });

    render_pass_builder.Finish();
}

void ForwardRenderer::RenderGui(rg::TextureHandle target, bool clear_target) {
    m_GuiTarget      = target;
    m_ClearGuiTarget = clear_target;
}

void ForwardRenderer::ToSwapChain(rg::TextureHandle from) {
    rg::PresentPassBuilder(m_RenderGraph)
        .From(from)
        .SetSwapChain(m_SwapChain)
        .Finish();
}

void ForwardRenderer::RecordMaterialInstance(rg::RenderPassBuilder& builder, const std::shared_ptr<asset::MaterialInstance>& material_instance) {
    if (m_MaterialInstanceInfos.contains(material_instance.get())) return;

    MaterialInstanceInfo info{
        .material_instance = material_instance,
        .samplers          = {m_Sampler},
    };

    for (const auto& texture : material_instance->GetAssociatedTextures()) {
        texture->InitGPUData(m_GfxDevice);
        builder.Read(
            info.textures.emplace_back(m_RenderGraph.Import(texture->GetGPUData(), texture->GetUniqueName())),
            {},
            gfx::PipelineStage::PixelShader);
    }

    m_MaterialInstanceInfos.emplace(material_instance.get(), std::move(info));
}

void ForwardRenderer::RecordMesh(rg::RenderPassBuilder& builder, const std::shared_ptr<asset::Mesh>& mesh) {
    if (m_MeshInfos.contains(mesh.get())) return;

    mesh->vertices->InitGPUData(m_GfxDevice);
    mesh->indices->InitGPUData(m_GfxDevice);

    utils::EnumArray<rg::GPUBufferHandle, asset::VertexAttribute> vertex_handles;
    magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
        auto attr_data = mesh->vertices->GetAttributeData(attr);
        if (!attr_data.has_value()) return;
        auto vertex_buffer_handle = m_RenderGraph.Import(attr_data->get().gpu_buffer);
        builder.ReadAsVertices(vertex_buffer_handle);
        vertex_handles[attr] = vertex_buffer_handle;
    });

    auto index_buffer_handle = m_RenderGraph.Import(mesh->indices->GetIndexData().gpu_buffer);
    builder.ReadAsIndices(index_buffer_handle);

    m_MeshInfos.emplace(
        mesh.get(),
        MeshInfo{
            .mesh     = mesh,
            .vertices = vertex_handles,
            .indices  = index_buffer_handle,
        });

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
    for (auto& [material_instance, info] : m_MaterialInstanceInfos) {
        const auto material          = material_instance->GetMaterial();
        info.material_instance_index = material_instance_counter[material]++;
    }

    for (const auto& [material, num_instances] : material_instance_counter) {
        auto pipeline_handle = m_RenderGraph.Import(material->GetPipeline(m_GfxDevice), material->GetName());
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
        .element_count = m_InstanceInfos.size(),
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
        .element_count = num_draws,
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
    m_MaterialInstanceInfos.clear();
    m_MeshInfos.clear();
    m_InstanceInfos.clear();
}

}  // namespace hitagi::render