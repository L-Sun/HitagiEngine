#include <hitagi/render/forward_renderer.hpp>
#include <hitagi/core/thread_manager.hpp>
#include <hitagi/application.hpp>
#include <hitagi/gfx/utils.hpp>

#include <spdlog/logger.h>
#include <tracy/Tracy.hpp>
#include <imgui.h>
#include <range/v3/all.hpp>

#undef near
#undef far

namespace hitagi::render {

ForwardRenderer::ForwardRenderer(const Application& app, gui::GuiManager* gui_manager, std::string_view name)
    : IRenderer(fmt::format("ForwardRenderer{}", name.empty() ? "" : fmt::format("({})", name))),
      m_App(app),
      m_GfxDevice(gfx::Device::Create(magic_enum::enum_cast<gfx::Device::Type>(app.GetConfig().gfx_backend).value(), name)),
      m_SwapChain(m_GfxDevice->CreateSwapChain({
          .name        = "SwapChain",
          .window      = app.GetWindow(),
          .clear_color = math::vec4f(0, 0, 0, 1),
      })),
      m_RenderGraph(*m_GfxDevice, "ForwardRenderGraph"),
      m_GuiRenderUtils(gui_manager ? std::make_unique<GuiRenderUtils>(*gui_manager, *m_GfxDevice) : nullptr)

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
        m_GfxDevice->Profile(m_RenderGraph.Execute());
    }

    m_SwapChain->Present();

    m_Clock.Tick();
}

void ForwardRenderer::RenderScene(std::shared_ptr<asset::Scene> scene, std::shared_ptr<asset::CameraNode> camera, rg::TextureHandle target) {
    ZoneScoped;

    struct FrameConstant {
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
        float       light_intensity;
    };
    struct InstanceConstant {
        math::mat4f model;
    };

    // TODO generate by material
    struct BindlessInfo {
        gfx::BindlessHandle                frame_constant;
        gfx::BindlessHandle                instance_constant;
        gfx::BindlessHandle                material_constant;
        std::array<gfx::BindlessHandle, 4> textures;
        gfx::BindlessHandle                sampler;
    };

    struct MaterialInfo {
        rg::RenderPipelineHandle pipeline;
        rg::GPUBufferHandle      material_constant;
    };

    struct MaterialInstanceInfo {
        MaterialInfo                        material_info;
        std::pmr::vector<rg::TextureHandle> textures;
        std::pmr::vector<rg::SamplerHandle> samplers;
        core::Buffer                        material_instance_data;
        std::size_t                         material_instance_index;
    };

    // one DrawItem corresponds to one SubMesh, so it only refers to one draw call
    struct DrawItem {
        std::size_t                                                 index_count;
        std::pmr::unordered_map<std::uint32_t, rg::GPUBufferHandle> vertices;
        std::size_t                                                 vertex_offset;
        rg::GPUBufferHandle                                         indices;
        std::size_t                                                 index_offset;
        InstanceConstant                                            instance_data;
        std::size_t                                                 instance_index;
        std::shared_ptr<MaterialInstanceInfo>                       material_instance_info;
    };

    const auto frame_constant_handle = m_RenderGraph.Create(
        {
            .name          = "frame_constant",
            .element_size  = sizeof(FrameConstant),
            .element_count = 1,
            .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
        });

    const auto instance_constant_handle = m_RenderGraph.Create({
        .name          = "instance_constant",
        .element_size  = sizeof(InstanceConstant),
        .element_count = scene->instance_nodes.size(),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });

    const auto sampler_handle = m_RenderGraph.Create(gfx::SamplerDesc{
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
        .Read(frame_constant_handle)
        .Read(instance_constant_handle)
        .AddSampler(sampler_handle);

    std::unordered_map<asset::Material*, MaterialInfo> material_infos;
    {
        ZoneScopedN("generate material infos");

        const auto materials = scene->instance_nodes                                                                          //
                               | ranges::views::transform([](const auto& node) { return node->GetObjectRef()->sub_meshes; })  //
                               | ranges::views::join                                                                          //
                               | ranges::views::transform([](const auto& sub_mesh) {
                                     return sub_mesh.material_instance->GetMaterial();
                                 })  //
                               | ranges::to<std::pmr::unordered_set<std::shared_ptr<asset::Material>>>();

        material_infos = materials  //
                         | ranges::views::transform([&](const auto& material) {
                               material->InitPipeline(*m_GfxDevice);
                               auto pipeline_handle = m_RenderGraph.Import(material->GetPipeline(), material->GetName());
                               render_pass_builder.AddPipeline(pipeline_handle);

                               auto constant_handle = m_RenderGraph.Create(
                                   {
                                       .name          = std::pmr::string(material->GetName()),
                                       .element_size  = material->CalculateMaterialBufferSize(),
                                       .element_count = material->GetInstances().size(),
                                       .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
                                   },
                                   material->GetName());
                               render_pass_builder.Read(constant_handle);

                               return std::make_pair(
                                   material.get(),
                                   MaterialInfo{
                                       .pipeline          = pipeline_handle,
                                       .material_constant = constant_handle,
                                   });
                           })  //
                         | ranges::to<decltype(material_infos)>();
    }

    std::unordered_map<asset::MaterialInstance*, std::shared_ptr<MaterialInstanceInfo>> material_instance_infos;
    {
        ZoneScopedN("generate material instance infos");

        for (const auto& [material, material_info] : material_infos) {
            for (const auto& [material_instance_index, material_instance] : material->GetInstances() | ranges::views::enumerate) {
                auto material_instance_info = std::make_shared<MaterialInstanceInfo>();

                material_instance_info->material_info           = material_info;
                material_instance_info->samplers                = {sampler_handle};
                material_instance_info->material_instance_data  = material_instance->GetMateriaBufferData();
                material_instance_info->material_instance_index = material_instance_index;

                for (const auto& texture : material_instance->GetTextures()) {
                    texture->InitGPUData(*m_GfxDevice);
                    render_pass_builder.Read(
                        material_instance_info->textures.emplace_back(m_RenderGraph.Import(texture->GetGPUData(), texture->GetUniqueName())),
                        {},
                        gfx::PipelineStage::PixelShader);
                }

                material_instance_infos.emplace(material_instance, std::move(material_instance_info));
            }
        }
    }

    // generate draw items
    std::pmr::vector<DrawItem> draw_items;
    {
        ZoneScopedN("generate draw items");

        {
            ZoneScopedN("reserve draw items");
            std::size_t num_draw_items = 0;
            for (const auto& node : scene->instance_nodes) {
                num_draw_items += node->GetObjectRef()->sub_meshes.size();
            }
            draw_items.reserve(num_draw_items);
        }

        for (const auto& [instant_index, node] : scene->instance_nodes | ranges::views::enumerate) {
            auto mesh = node->GetObjectRef();
            mesh->vertices->InitGPUData(*m_GfxDevice);
            mesh->indices->InitGPUData(*m_GfxDevice);

            utils::EnumArray<rg::GPUBufferHandle, asset::VertexAttribute> vertex_handles;
            magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
                auto attr_data = mesh->vertices->GetAttributeData(attr);
                if (!attr_data.has_value()) return;
                auto vertex_buffer_handle = m_RenderGraph.Import(attr_data->get().gpu_buffer);
                render_pass_builder.ReadAsVertices(vertex_buffer_handle);
                vertex_handles[attr] = vertex_buffer_handle;
            });

            auto index_buffer_handle = m_RenderGraph.Import(mesh->indices->GetIndexData().gpu_buffer);
            render_pass_builder.ReadAsIndices(index_buffer_handle);

            for (const auto& sub_mesh : mesh->sub_meshes) {
                ZoneScopedN("add draw item");

                auto material = sub_mesh.material_instance->GetMaterial();

                const auto& pipeline = material->GetPipeline();

                std::pmr::unordered_map<std::uint32_t, rg::GPUBufferHandle> binding_to_vertex_handles;
                for (const auto& vertex_attr : pipeline->GetDesc().vertex_input_layout) {
                    binding_to_vertex_handles.emplace(vertex_attr.binding, vertex_handles[asset::semantic_to_vertex_attribute(vertex_attr.semantic)]);
                }

                draw_items.emplace_back(DrawItem{
                    .index_count   = sub_mesh.index_count,
                    .vertices      = std::move(binding_to_vertex_handles),
                    .vertex_offset = sub_mesh.vertex_offset,
                    .indices       = index_buffer_handle,
                    .index_offset  = sub_mesh.index_offset,
                    .instance_data = {
                        .model = node->transform.world_matrix,
                    },
                    .instance_index         = instant_index,
                    .material_instance_info = material_instance_infos.at(sub_mesh.material_instance.get()),
                });
            }
        }

        {
            ZoneScopedN("sort draw items");

            std::sort(draw_items.begin(), draw_items.end(), [](const auto& lhs, const auto& rhs) -> bool {
                return lhs.material_instance_info->material_info.pipeline < rhs.material_instance_info->material_info.pipeline;
            });
        }
    }

    const auto bindless_infos_handle = m_RenderGraph.Create({
        .name          = "bindless_infos",
        .element_size  = sizeof(BindlessInfo),
        .element_count = draw_items.size(),
        .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
    });
    render_pass_builder.Read(bindless_infos_handle);

    render_pass_builder.SetExecutor([=, _draw_items = std::move(draw_items)](const rg::RenderGraph& render_graph, const rg::RenderPassNode& pass) {
        auto& cmd = pass.GetCmd();

        // update frame constant
        {
            auto frame_constant    = gfx::GPUBufferView<FrameConstant>(pass.Resolve(frame_constant_handle));
            frame_constant.front() = FrameConstant{
                .camera_pos     = {camera->transform.GetPosition(), 1.0f},
                .view           = camera->GetView(),
                .projection     = camera->GetProjection(),
                .proj_view      = camera->GetProjectionView(),
                .inv_view       = camera->GetInvView(),
                .inv_projection = camera->GetInvProjection(),
                .inv_proj_view  = camera->GetInvProjectionView(),
            };

            if (!scene->light_nodes.empty()) {
                auto light_node = scene->light_nodes.front();

                frame_constant.front().light_position    = {light_node->GetLightGlobalPosition(), 1.0f};
                frame_constant.front().light_pos_in_view = camera->GetView() * math::vec4f(light_node->GetLightGlobalPosition(), 1.0f);
                frame_constant.front().light_color       = light_node->GetObjectRef()->parameters.color;
                frame_constant.front().light_intensity   = light_node->GetObjectRef()->parameters.intensity;
            }
        }

        gfx::GPUBufferView<BindlessInfo> bindless_infos(pass.Resolve(bindless_infos_handle));

        const auto& render_target = pass.Resolve(target);
        cmd.SetViewPort({0, 0, static_cast<float>(render_target.GetDesc().width), static_cast<float>(render_target.GetDesc().height)});
        cmd.SetScissorRect({0, 0, render_target.GetDesc().width, render_target.GetDesc().height});

        for (const auto& [draw_index, draw_item] : _draw_items | ranges::views::enumerate) {
            const auto& material_instance_info = draw_item.material_instance_info;

            auto& pipeline = pass.Resolve(material_instance_info->material_info.pipeline);
            cmd.SetPipeline(pipeline);

            const auto material_constant_handle = material_instance_info->material_info.material_constant;
            const auto material_instance_index  = material_instance_info->material_instance_index;

            // Update material instance data
            {
                auto& material_constant_buffer = pass.Resolve(material_constant_handle);
                std::memcpy(material_constant_buffer.Map() + material_constant_buffer.AlignedElementSize() * material_instance_index,
                            material_instance_info->material_instance_data.GetData(),
                            material_instance_info->material_instance_data.GetDataSize());
                material_constant_buffer.UnMap();
            }

            // update instance constant
            {
                auto instance_constant                      = gfx::GPUBufferView<InstanceConstant>(pass.Resolve(instance_constant_handle));
                instance_constant[draw_item.instance_index] = draw_item.instance_data;
            }

            bindless_infos[draw_index] = BindlessInfo{
                .frame_constant    = pass.GetBindless(frame_constant_handle),
                .instance_constant = pass.GetBindless(instance_constant_handle, draw_item.instance_index),
                .material_constant = pass.GetBindless(material_constant_handle, material_instance_index),
                .textures          = {
                    pass.GetBindless(material_instance_info->textures[0]),
                    pass.GetBindless(material_instance_info->textures[1]),
                    pass.GetBindless(material_instance_info->textures[2]),
                    pass.GetBindless(material_instance_info->textures[3]),
                },
                .sampler = pass.GetBindless(material_instance_info->samplers[0]),
            };
            cmd.PushBindlessMetaInfo({
                .handle = pass.GetBindless(bindless_infos_handle, draw_index),
            });
            for (const auto& vertex_attr : pipeline.GetDesc().vertex_input_layout) {
                cmd.SetVertexBuffers(vertex_attr.binding, {{pass.Resolve(draw_item.vertices.at(vertex_attr.binding))}}, {{0}});
            }
            cmd.SetIndexBuffer(pass.Resolve(draw_item.indices), 0);

            cmd.DrawIndexed(draw_item.index_count, 1, draw_item.index_offset, draw_item.vertex_offset);
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

}  // namespace hitagi::render