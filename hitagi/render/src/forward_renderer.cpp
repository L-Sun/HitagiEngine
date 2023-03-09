#include <hitagi/render/forward_renderer.hpp>
#include <hitagi/core/thread_manager.hpp>
#include <hitagi/application.hpp>

#include <spdlog/logger.h>
#include <tracy/Tracy.hpp>
#include <imgui.h>

#undef near
#undef far

namespace hitagi::render {

ForwardRenderer::ForwardRenderer(const Application& app, gfx::Device::Type gfx_device_type, gui::GuiManager* gui_manager)
    : IRenderer("ForwardRenderer"),
      m_App(app),
      m_GfxDevice(gfx::Device::Create(gfx_device_type)),
      m_SwapChain(m_GfxDevice->CreateSwapChain({
          .name        = "SwapChain",
          .window      = app.GetWindow(),
          .frame_count = 2,
          .format      = gfx::Format::R8G8B8A8_UNORM,
      })),
      m_RenderGraph(*m_GfxDevice),
      m_LastFenceValues(utils::create_enum_array<std::uint64_t, gfx::CommandType>(0)),
      m_GuiRenderUtils(gui_manager ? std::make_unique<GuiRenderUtils>(*gui_manager, *m_GfxDevice) : nullptr) {
    m_Clock.Start();
    ClearPass();
}

ForwardRenderer::~ForwardRenderer() { m_GfxDevice->WaitIdle(); }

void ForwardRenderer::Tick() {
    m_RenderGraph.PresentPass(m_RenderGraph.GetImportedResourceHandle("BackBuffer"));

    if (thread_manager) {
        auto compile         = thread_manager->RunTask([this]() {
            ZoneScopedN("RenderGraph Compile");
            return m_RenderGraph.Compile();
        });
        auto wait_last_frame = thread_manager->RunTask([this]() {
            ZoneScopedN("Wait Last Frame");
            magic_enum::enum_for_each<gfx::CommandType>([this](auto type) {
                m_GfxDevice->GetCommandQueue(type()).WaitForFence(m_LastFenceValues[type()]);
            });
        });
        compile.wait();
        wait_last_frame.wait();
    } else {
        {
            ZoneScopedN("RenderGraph Compile");
            m_RenderGraph.Compile();
        }
        {
            ZoneScopedN("Wait Last Frame");
            magic_enum::enum_for_each<gfx::CommandType>([this](auto type) {
                m_GfxDevice->GetCommandQueue(type()).WaitForFence(m_LastFenceValues[type()]);
            });
        }
    }

    m_LastFenceValues = m_RenderGraph.Execute();
    m_RenderGraph.Reset();
    m_ColorPass = {};

    {
        ZoneScopedN("Present");
        m_SwapChain->Present();
    }

    if (m_App.WindowSizeChanged()) {
        m_SwapChain->Resize();
    }
    m_FrameIndex++;

    m_GfxDevice->Profile(m_FrameIndex);
    m_Clock.Tick();

    ClearPass();
}

auto ForwardRenderer::RenderScene(const asset::Scene& scene, const asset::CameraNode& camera, std::optional<gfx::ViewPort> viewport, std::optional<math::vec2u> texture_size) -> gfx::ResourceHandle {
    struct DrawItem {
        std::shared_ptr<asset::MeshNode>    instance;
        std::shared_ptr<asset::Material>    material;
        std::shared_ptr<asset::VertexArray> vertices;
        std::shared_ptr<asset::IndexArray>  inidices;
        asset::Mesh::SubMesh                submesh;
    };
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

    // use shared_ptr to avoid unnecessory copying
    auto draw_items = std::make_shared<std::pmr::vector<DrawItem>>();

    std::pmr::set<std::shared_ptr<asset::Material>> materials;

    for (const auto& node : scene.instance_nodes) {
        auto mesh = node->GetObjectRef();
        mesh->vertices->InitGpuData(*m_GfxDevice);
        mesh->indices->InitGpuData(*m_GfxDevice);

        for (const auto& submesh : mesh->sub_meshes) {
            auto material = submesh.material_instance->GetMaterial();
            material->InitPipeline(*m_GfxDevice);

            for (const auto& texture : submesh.material_instance->GetTextures()) {
                texture->InitGpuData(*m_GfxDevice);
            }

            materials.emplace(material);
            draw_items->emplace_back(DrawItem{
                .instance = node,
                .material = material,
                .vertices = mesh->vertices,
                .inidices = mesh->indices,
                .submesh  = submesh,
            });
        }
    }

    auto lignt_node = scene.light_nodes.empty() ? nullptr : scene.light_nodes.front();

    FrameConstant frame_constant = {
        .camera_pos     = math::vec4f(camera.transform.GetPosition(), 1.0f),
        .view           = camera.GetView(),
        .projection     = camera.GetProjection(),
        .proj_view      = camera.GetProjectionView(),
        .inv_view       = camera.GetInvView(),
        .inv_projection = camera.GetInvProjection(),
        .inv_proj_view  = camera.GetInvProjectionView(),
        // multiple light,
        .light_position    = lignt_node ? math::vec4f(lignt_node->GetLightGlobalPosition(), 1.0f) : math::vec4f(0, 0, 0, 0),
        .light_pos_in_view = lignt_node ? camera.GetView() * math::vec4f(lignt_node->GetLightGlobalPosition(), 1.0f) : math::vec4f(0, 0, 0, 0),
        .light_color       = lignt_node ? lignt_node->GetObjectRef()->parameters.color : math::vec4f(0, 0, 0, 0),
        .light_intensity   = lignt_node ? lignt_node->GetObjectRef()->parameters.intensity : 0,
    };

    std::sort(draw_items->begin(), draw_items->end(), [](const auto& lhs, const auto& rhs) -> bool {
        return lhs.material < rhs.material;
    });

    m_ColorPass = m_RenderGraph.AddPass<ColorPass>(
        "ColorPass",
        [&](gfx::RenderGraph::Builder& builder, ColorPass& data) {
            if (texture_size.has_value()) {
                data.color = builder.Create(gfx::Texture::Desc{
                    .name   = "scene-output",
                    .width  = static_cast<std::uint32_t>(texture_size->x),
                    .height = static_cast<std::uint32_t>(texture_size->y),
                    .format = gfx::Format::R8G8B8A8_UNORM,
                    .usages = gfx::Texture::UsageFlags::SRV | gfx::Texture::UsageFlags::RTV,
                });
                data.depth = builder.Create(gfx::Texture::Desc{
                    .name        = "scene-depth",
                    .width       = static_cast<std::uint32_t>(texture_size->x),
                    .height      = static_cast<std::uint32_t>(texture_size->y),
                    .format      = gfx::Format::D16_UNORM,
                    .clear_value = {
                        .depth   = 1.0f,
                        .stencil = 0,
                    },
                    .usages = gfx::Texture::UsageFlags::DSV,
                });
            } else {
                data.color = builder.Write(m_BackBufferHandle);
                data.depth = builder.Create(gfx::Texture::Desc{
                    .name        = "scene-depth",
                    .width       = static_cast<std::uint32_t>(m_SwapChain->Width()),
                    .height      = static_cast<std::uint32_t>(m_SwapChain->Height()),
                    .format      = gfx::Format::D16_UNORM,
                    .clear_value = {
                        .depth   = 1.0f,
                        .stencil = 0,
                    },
                    .usages = gfx::Texture::UsageFlags::DSV,
                });
            }

            data.frame_constant = builder.Create(gfx::GpuBuffer::Desc{
                .name         = "frame-constant",
                .element_size = sizeof(FrameConstant),
                .usages       = gfx::GpuBuffer::UsageFlags::MapWrite | gfx::GpuBuffer::UsageFlags::Constant,
            });

            data.instance_constant = builder.Create(gfx::GpuBuffer::Desc{
                .name          = "instant-constant",
                .element_size  = sizeof(InstanceConstant),
                .element_count = scene.instance_nodes.size(),
                .usages        = gfx::GpuBuffer::UsageFlags::MapWrite | gfx::GpuBuffer::UsageFlags::Constant,
            });

            for (const auto& material : materials) {
                data.material_constants[material.get()] = builder.Create(gfx::GpuBuffer::Desc{
                    .name          = material->GetName(),
                    .element_size  = material->CalculateMaterialBufferSize(),
                    .element_count = material->GetNumInstances(),
                    .usages        = gfx::GpuBuffer::UsageFlags::MapWrite | gfx::GpuBuffer::UsageFlags::Constant,
                });
            }

            for (const auto& item : *draw_items) {
                builder.UseRenderPipeline(item.material->GetPipeline());
                // FIXME: need match the pipeline
                magic_enum::enum_for_each<asset::VertexAttribute>([&](asset::VertexAttribute attr) {
                    auto attribute_data = item.vertices->GetAttributeData(attr);
                    if (attribute_data.has_value()) {
                        builder.Read(m_RenderGraph.Import(item.vertices->GetUniqueName(), attribute_data->get().gpu_buffer));
                    }
                });
                builder.Read(m_RenderGraph.Import(item.inidices->GetUniqueName(), item.inidices->GetIndexData().gpu_buffer));
                for (const auto& texture : item.submesh.material_instance->GetTextures()) {
                    builder.Read(m_RenderGraph.Import(texture->GetUniqueName(), texture->GetGpuData()));
                }
            }
        },
        [=](const gfx::RenderGraph::ResourceHelper& helper, const ColorPass& data, gfx::GraphicsCommandContext* context) mutable {
            helper.Get<gfx::GpuBuffer>(data.frame_constant).Update(0, frame_constant);

            auto& instance_constant = helper.Get<gfx::GpuBuffer>(data.instance_constant);

            std::pmr::unordered_map<asset::MeshNode*, std::size_t> instance_constant_offset;
            {
                std::size_t offset = 0;
                for (const auto& draw_call : *draw_items) {
                    if (instance_constant_offset.contains(draw_call.instance.get())) continue;
                    instance_constant.Update(
                        offset,
                        InstanceConstant{
                            .model = draw_call.instance->transform.world_matrix,
                        });
                    instance_constant_offset.emplace(draw_call.instance.get(), offset++);
                }
            }

            std::pmr::unordered_map<asset::MaterialInstance*, std::size_t> material_constant_offset;
            {
                material_constant_offset.reserve(draw_items->size());
                std::size_t offset = 0;
                for (const auto& draw_call : *draw_items) {
                    auto material_instance = draw_call.submesh.material_instance.get();
                    auto material          = draw_call.material.get();
                    if (material_constant_offset.contains(material_instance)) continue;
                    helper.Get<gfx::GpuBuffer>(data.material_constants.at(material))
                        .Update(
                            offset,
                            draw_call.submesh.material_instance->GetMateriaBufferData().Span<const std::byte>());

                    material_constant_offset.emplace(material_instance, offset++);
                }
            }

            auto& render_target = helper.Get<gfx::Texture>(data.color);
            auto& depth_buffer  = helper.Get<gfx::Texture>(data.depth);
            context->SetRenderTargetAndDepthStencil(render_target, depth_buffer);
            context->ClearRenderTarget(render_target);
            context->ClearDepthStencil(depth_buffer);

            if (!viewport.has_value()) {
                viewport = gfx::ViewPort{
                    .x      = 0,
                    .y      = 0,
                    .width  = static_cast<float>(render_target.GetDesc().width),
                    .height = static_cast<float>(render_target.GetDesc().height),
                };
            }

            context->SetViewPort(viewport.value());
            context->SetScissorRect(gfx::Rect{
                .x      = static_cast<std::uint32_t>(viewport->x),
                .y      = static_cast<std::uint32_t>(viewport->y),
                .width  = static_cast<std::uint32_t>(viewport->width),
                .height = static_cast<std::uint32_t>(viewport->height)});

            gfx::RenderPipeline* curr_pipeline     = nullptr;
            asset::VertexArray*  curr_vertices     = nullptr;
            asset::IndexArray*   curr_indices      = nullptr;
            asset::MeshNode*     curr_instance     = nullptr;
            gfx::GpuBuffer*      material_constant = nullptr;
            for (const auto& draw_call : *draw_items) {
                if (draw_call.material->GetPipeline() == nullptr) continue;

                if (curr_pipeline != draw_call.material->GetPipeline().get()) {
                    curr_pipeline = draw_call.material->GetPipeline().get();
                    context->SetPipeline(*curr_pipeline);
                    context->BindConstantBuffer(0, helper.Get<gfx::GpuBuffer>(data.frame_constant));
                    material_constant = &helper.Get<gfx::GpuBuffer>(data.material_constants.at(draw_call.material.get()));
                }
                if (curr_vertices != draw_call.vertices.get()) {
                    curr_vertices = draw_call.vertices.get();
                    for (const auto& vertex_attr : curr_pipeline->GetDesc().input_layout) {
                        auto attribute_data = curr_vertices->GetAttributeData(vertex_attr);
                        if (attribute_data.has_value()) {
                            context->SetVertexBuffer(vertex_attr.slot, *attribute_data->get().gpu_buffer);
                        }
                    }
                }
                if (curr_indices != draw_call.inidices.get()) {
                    curr_indices = draw_call.inidices.get();
                    context->SetIndexBuffer(*curr_indices->GetIndexData().gpu_buffer);
                }
                if (curr_instance != draw_call.instance.get()) {
                    curr_instance = draw_call.instance.get();
                    context->BindConstantBuffer(1, instance_constant, instance_constant_offset.at(curr_instance));
                }
                context->BindConstantBuffer(2, *material_constant, material_constant_offset.at(draw_call.submesh.material_instance.get()));

                for (std::size_t texture_index = 0; const auto& texture : draw_call.submesh.material_instance->GetTextures()) {
                    context->BindTexture(texture_index++, *texture->GetGpuData());
                }

                context->DrawIndexed(
                    draw_call.submesh.index_count,
                    1,
                    draw_call.submesh.index_offset,
                    draw_call.submesh.vertex_offset);
            }
        });

    if (!texture_size.has_value()) {
        m_BackBufferHandle = m_ColorPass.color;
    }
    return m_ColorPass.color;
}

auto ForwardRenderer::RenderGui(std::optional<gfx::ResourceHandle> target) -> gfx::ResourceHandle {
    auto gui_pass = m_GuiRenderUtils->GuiPass(m_RenderGraph, target.has_value() ? target.value() : m_BackBufferHandle);

    if (target.has_value()) {
        m_BackBufferHandle = gui_pass;
    }

    return gui_pass;
}

void ForwardRenderer::ClearPass() {
    m_BackBufferHandle = m_RenderGraph.AddPass<gfx::ResourceHandle>(
        "ClearPass",
        [&](gfx::RenderGraph::Builder& builder, auto& cleared_back_buffer) {
            cleared_back_buffer = builder.Write(m_RenderGraph.ImportWithoutLifeTrack("BackBuffer", &m_SwapChain->GetCurrentBackBuffer()));
        },
        [=](const gfx::RenderGraph::ResourceHelper& helper, auto& cleared_back_buffer, gfx::GraphicsCommandContext* context) {
            context->SetRenderTarget(helper.Get<gfx::Texture>(cleared_back_buffer));
            context->ClearRenderTarget(helper.Get<gfx::Texture>(cleared_back_buffer));
        });
}

}  // namespace hitagi::render