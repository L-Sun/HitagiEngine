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
          .name   = "SwapChain",
          .window = app.GetWindow(),
      })),
      m_RenderGraph(*m_GfxDevice),
      m_GuiRenderUtils(gui_manager ? std::make_unique<GuiRenderUtils>(*gui_manager, *m_GfxDevice) : nullptr) {
    m_Clock.Start();
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
                m_GfxDevice->GetCommandQueue(type);
            });
        });
        compile.wait();
        wait_last_frame.wait();
    } else {
        {
            ZoneScopedN("RenderGraph Compile");
            m_RenderGraph.Compile();
        }
    }

    m_RenderGraph.Execute();
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
}

auto ForwardRenderer::RenderScene(const asset::Scene& scene, const asset::CameraNode& camera, std::optional<gfx::ViewPort> viewport, std::optional<math::vec2u> texture_size) -> gfx::ResourceHandle {
    struct DrawItem {
        std::shared_ptr<asset::MeshNode>    instance;
        std::shared_ptr<asset::Material>    material;
        std::shared_ptr<asset::VertexArray> vertices;
        std::shared_ptr<asset::IndexArray>  indices;
        asset::Mesh::SubMesh                sub_mesh;
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

    struct BindlessInfo {
        gfx::BindlessHandle                frame_constant;
        gfx::BindlessHandle                instance_constant;
        gfx::BindlessHandle                material;
        std::array<gfx::BindlessHandle, 4> textures;
        gfx::BindlessHandle                sampler;
    };

    // use shared_ptr to avoid unnecessary copying
    auto draw_items = std::make_shared<std::pmr::vector<DrawItem>>();

    std::pmr::set<std::shared_ptr<asset::Material>> materials;

    for (const auto& node : scene.instance_nodes) {
        auto mesh = node->GetObjectRef();
        mesh->vertices->InitGPUData(*m_GfxDevice);
        mesh->indices->InitGPUData(*m_GfxDevice);

        for (const auto& sub_mesh : mesh->sub_meshes) {
            auto material = sub_mesh.material_instance->GetMaterial();
            material->InitPipeline(*m_GfxDevice);

            for (const auto& texture : sub_mesh.material_instance->GetTextures()) {
                texture->InitGPUData(*m_GfxDevice);
            }

            materials.emplace(material);
            draw_items->emplace_back(DrawItem{
                .instance = node,
                .material = material,
                .vertices = mesh->vertices,
                .indices  = mesh->indices,
                .sub_mesh = sub_mesh,
            });
        }
    }

    auto light_node = scene.light_nodes.empty() ? nullptr : scene.light_nodes.front();

    FrameConstant frame_constant = {
        .camera_pos     = math::vec4f(camera.transform.GetPosition(), 1.0f),
        .view           = camera.GetView(),
        .projection     = camera.GetProjection(),
        .proj_view      = camera.GetProjectionView(),
        .inv_view       = camera.GetInvView(),
        .inv_projection = camera.GetInvProjection(),
        .inv_proj_view  = camera.GetInvProjectionView(),
        // multiple light,
        .light_position    = light_node ? math::vec4f(light_node->GetLightGlobalPosition(), 1.0f) : math::vec4f(0, 0, 0, 0),
        .light_pos_in_view = light_node ? camera.GetView() * math::vec4f(light_node->GetLightGlobalPosition(), 1.0f) : math::vec4f(0, 0, 0, 0),
        .light_color       = light_node ? light_node->GetObjectRef()->parameters.color : math::vec4f(0, 0, 0, 0),
        .light_intensity   = light_node ? light_node->GetObjectRef()->parameters.intensity : 0,
    };

    std::sort(draw_items->begin(), draw_items->end(), [](const auto& lhs, const auto& rhs) -> bool {
        return lhs.material < rhs.material;
    });

    m_ColorPass = m_RenderGraph.AddPass<ColorPass>(
        "ColorPass",
        [&](gfx::RenderGraph::Builder& builder, ColorPass& data) {
            if (texture_size.has_value()) {
                data.color = builder.Create(gfx::TextureDesc{
                    .name   = "scene-output",
                    .width  = static_cast<std::uint32_t>(texture_size->x),
                    .height = static_cast<std::uint32_t>(texture_size->y),
                    .format = gfx::Format::R8G8B8A8_UNORM,
                    .usages = gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::RTV,
                });
                data.depth = builder.Create(gfx::TextureDesc{
                    .name        = "scene-depth",
                    .width       = static_cast<std::uint32_t>(texture_size->x),
                    .height      = static_cast<std::uint32_t>(texture_size->y),
                    .format      = gfx::Format::D16_UNORM,
                    .clear_value = gfx::ClearDepthStencil{
                        .depth   = 1.0f,
                        .stencil = 0,
                    },
                    .usages = gfx::TextureUsageFlags::DSV,
                });
            } else {
                data.color = builder.Write(m_SwapChianHandle);
                data.depth = builder.Create(gfx::TextureDesc{
                    .name        = "scene-depth",
                    .width       = static_cast<std::uint32_t>(m_SwapChain->GetWidth()),
                    .height      = static_cast<std::uint32_t>(m_SwapChain->GetHeight()),
                    .format      = gfx::Format::D16_UNORM,
                    .clear_value = gfx::ClearDepthStencil{
                        .depth   = 1.0f,
                        .stencil = 0,
                    },
                    .usages = gfx::TextureUsageFlags::DSV,
                });
            }

            data.bindless_info_buffer = builder.Create(gfx::GPUBufferDesc{
                .name         = "bindless-info",
                .element_size = sizeof(BindlessInfo),
                .usages       = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
            });

            data.frame_constant = builder.Create(gfx::GPUBufferDesc{
                .name         = "frame-constant",
                .element_size = sizeof(FrameConstant),
                .usages       = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
            });

            data.instance_constant = builder.Create(gfx::GPUBufferDesc{
                .name          = "instant-constant",
                .element_size  = sizeof(InstanceConstant),
                .element_count = scene.instance_nodes.size(),
                .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
            });

            for (const auto& material : materials) {
                data.material_constants[material.get()] = builder.Create(gfx::GPUBufferDesc{
                    .name          = material->GetName(),
                    .element_size  = material->CalculateMaterialBufferSize(),
                    .element_count = material->GetNumInstances(),
                    .usages        = gfx::GPUBufferUsageFlags::MapWrite | gfx::GPUBufferUsageFlags::Constant,
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
                builder.Read(m_RenderGraph.Import(item.indices->GetUniqueName(), item.indices->GetIndexData().gpu_buffer));
                for (const auto& texture : item.sub_mesh.material_instance->GetTextures()) {
                    builder.Read(m_RenderGraph.Import(texture->GetUniqueName(), texture->GetGPUData()));
                }
            }
        },
        [=](const gfx::RenderGraph::ResourceHelper& helper, const ColorPass& data, gfx::GraphicsCommandContext* context) mutable {
            helper.Get<gfx::GPUBuffer>(data.frame_constant).Update(0, frame_constant);

            auto& bindless_info = helper.Get<gfx::GPUBuffer>(data.bindless_info_buffer);

            auto& instance_constant = helper.Get<gfx::GPUBuffer>(data.instance_constant);

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
                    auto material_instance = draw_call.sub_mesh.material_instance.get();
                    auto material          = draw_call.material.get();
                    if (material_constant_offset.contains(material_instance)) continue;
                    helper.Get<gfx::GPUBuffer>(data.material_constants.at(material))
                        .Update(
                            offset,
                            draw_call.sub_mesh.material_instance->GetMateriaBufferData().Span<const std::byte>());

                    material_constant_offset.emplace(material_instance, offset++);
                }
            }

            auto& render_target = helper.Get<gfx::Texture>(data.color);
            auto& depth_buffer  = helper.Get<gfx::Texture>(data.depth);
            context->BeginRendering({
                .render_target = render_target,
                .depth_stencil = depth_buffer,
            });

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

            for (const auto& draw_call : *draw_items) {
                if (draw_call.material->GetPipeline() == nullptr) continue;

                context->SetPipeline(*draw_call.material->GetPipeline());

                for (const auto& vertex_attr : draw_call.material->GetPipeline()->GetDesc().vertex_input_layout) {
                    auto attribute_data = draw_call.vertices->GetAttributeData(vertex_attr);
                    if (attribute_data.has_value()) {
                        context->SetVertexBuffer(vertex_attr.binding, *attribute_data->get().gpu_buffer);
                    }
                }
                context->SetIndexBuffer(*draw_call.indices->GetIndexData().gpu_buffer);

                for (const auto& texture : draw_call.sub_mesh.material_instance->GetTextures()) {
                }

                context->DrawIndexed(
                    draw_call.sub_mesh.index_count,
                    1,
                    draw_call.sub_mesh.index_offset,
                    draw_call.sub_mesh.vertex_offset);
            }
        });

    if (!texture_size.has_value()) {
        m_SwapChianHandle = m_ColorPass.color;
    }
    return m_ColorPass.color;
}

auto ForwardRenderer::RenderGui(std::optional<gfx::ResourceHandle> target) -> gfx::ResourceHandle {
    auto gui_pass = m_GuiRenderUtils->GuiPass(m_RenderGraph, target.has_value() ? target.value() : m_SwapChianHandle);

    if (target.has_value()) {
        m_SwapChianHandle = gui_pass;
    }

    return gui_pass;
}

}  // namespace hitagi::render