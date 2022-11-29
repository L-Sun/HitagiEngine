#include <hitagi/asset/scene.hpp>

#include <tracy/Tracy.hpp>

#include <vector>

namespace hitagi::asset {
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

Scene::Scene(std::string_view name, xg::Guid guid)
    : Resource(name, guid),
      world(name) {
    world.RegisterSystem<TransformSystem>("TransformSystem");
}

void Scene::Update() {
    world.Update();
}

auto Scene::Render(gfx::RenderGraph& render_graph, gfx::ViewPort viewport, const std::shared_ptr<CameraNode>& camera) -> RenderPass {
    ZoneScopedN("Scene::Render");

    if (camera != nullptr) curr_camera = camera;

    root->Update();

    struct DrawItem {
        std::shared_ptr<MeshNode>    instance;
        std::shared_ptr<Material>    material;
        std::shared_ptr<VertexArray> vertices;
        std::shared_ptr<IndexArray>  inidices;
        Mesh::SubMesh                submesh;
    };

    std::pmr::vector<DrawItem>               draw_items;
    std::pmr::set<std::shared_ptr<Material>> materials;
    for (const auto& node : instance_nodes) {
        auto mesh = node->GetObjectRef();
        mesh->GetVertexArray()->InitGpuData(render_graph.device);
        mesh->GetIndexArray()->InitGpuData(render_graph.device);

        for (const auto& submesh : mesh->GetSubMeshes()) {
            auto material = submesh.material_instance->GetMaterial();
            material->InitPipeline(render_graph.device);

            for (const auto& texture : submesh.material_instance->GetTextures()) {
                texture->InitGpuData(render_graph.device);
            }

            materials.emplace(material);
            draw_items.emplace_back(DrawItem{
                .instance = node,
                .material = material,
                .vertices = mesh->GetVertexArray(),
                .inidices = mesh->GetIndexArray(),
                .submesh  = submesh,
            });
        }
    }

    FrameConstant frame_constant = {
        .camera_pos     = math::vec4f(curr_camera->transform.GetPosition(), 1.0f),
        .view           = curr_camera->GetView(),
        .projection     = curr_camera->GetProjection(),
        .proj_view      = curr_camera->GetProjectionView(),
        .inv_view       = curr_camera->GetInvView(),
        .inv_projection = curr_camera->GetInvProjection(),
        .inv_proj_view  = curr_camera->GetInvProjectionView(),
        // multiple light,
        .light_position    = math::vec4f(light_nodes.front()->GetLightGlobalPosition(), 1.0f),
        .light_pos_in_view = curr_camera->GetView() * math::vec4f(light_nodes.front()->GetLightGlobalPosition(), 1.0f),
        .light_color       = light_nodes.front()->GetObjectRef()->parameters.color,
        .light_intensity   = light_nodes.front()->GetObjectRef()->parameters.intensity,
    };

    std::sort(draw_items.begin(), draw_items.end(), [](const auto& lhs, const auto& rhs) -> bool {
        return lhs.material < rhs.material;
    });

    return render_graph.AddPass<RenderPass>(
        "ColorPass",
        [&](gfx::RenderGraph::Builder& builder, RenderPass& data) {
            data.render_target = builder.Create(gfx::Texture::Desc{
                .name   = "scene-output",
                .width  = static_cast<std::uint32_t>(viewport.width),
                .height = static_cast<std::uint32_t>(viewport.height),
                .format = gfx::Format::R8G8B8A8_UNORM,
                .usages = gfx::Texture::UsageFlags::SRV | gfx::Texture::UsageFlags::RTV,
            });

            data.depth_buffer = builder.Create(gfx::Texture::Desc{
                .name        = "scene-depth",
                .width       = static_cast<std::uint32_t>(viewport.width),
                .height      = static_cast<std::uint32_t>(viewport.height),
                .format      = gfx::Format::D32_FLOAT,
                .clear_value = {
                    .depth   = 1.0f,
                    .stencil = 0,
                },
                .usages = gfx::Texture::UsageFlags::DSV,
            });

            data.frame_constant = builder.Create(gfx::GpuBuffer::Desc{
                .name         = "frame-constant",
                .element_size = sizeof(FrameConstant),
                .usages       = gfx::GpuBuffer::UsageFlags::MapWrite | gfx::GpuBuffer::UsageFlags::Constant,
            });

            data.instance_constant = builder.Create(gfx::GpuBuffer::Desc{
                .name          = "instant-constant",
                .element_size  = sizeof(InstanceConstant),
                .element_count = instance_nodes.size(),
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

            for (const auto& item : draw_items) {
                magic_enum::enum_for_each<VertexAttribute>([&](VertexAttribute attr) {
                    auto attribute_data = item.vertices->GetAttributeData(attr);
                    if (attribute_data.has_value()) {
                        builder.Read(render_graph.Import(item.vertices->GetName(), attribute_data->get().gpu_buffer));
                    }
                });
                builder.Read(render_graph.Import(item.inidices->GetName(), item.inidices->GetIndexData().gpu_buffer));
                for (const auto& texture : item.submesh.material_instance->GetTextures()) {
                    builder.Read(render_graph.Import(texture->GetName(), texture->GetGpuData()));
                }
            }
        },
        [=, draw_calls = std::move(draw_items)](const gfx::RenderGraph::ResourceHelper& helper, const RenderPass& data, gfx::GraphicsCommandContext* context) {
            helper.Get<gfx::GpuBuffer>(data.frame_constant).Update(0, frame_constant);

            auto& instance_constant = helper.Get<gfx::GpuBuffer>(data.instance_constant);

            std::pmr::unordered_map<MeshNode*, std::size_t> instance_constant_offset;
            {
                std::size_t offset = 0;
                for (const auto& draw_call : draw_calls) {
                    if (instance_constant_offset.contains(draw_call.instance.get())) continue;
                    instance_constant.Update(
                        offset,
                        InstanceConstant{
                            .model = draw_call.instance->transform.world_matrix,
                        });
                    instance_constant_offset.emplace(draw_call.instance.get(), offset++);
                }
            }

            std::pmr::unordered_map<MaterialInstance*, std::size_t> material_constant_offset;
            {
                material_constant_offset.reserve(draw_calls.size());
                std::size_t offset = 0;
                for (const auto& draw_call : draw_calls) {
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

            context->SetViewPort(viewport);
            context->SetScissorRect(gfx::Rect{
                .x      = static_cast<std::uint32_t>(viewport.x),
                .y      = static_cast<std::uint32_t>(viewport.y),
                .width  = static_cast<std::uint32_t>(viewport.width),
                .height = static_cast<std::uint32_t>(viewport.height)});

            auto& render_target = helper.Get<gfx::Texture>(data.render_target);
            auto& depth_buffer  = helper.Get<gfx::Texture>(data.depth_buffer);
            context->SetRenderTargetAndDepthStencil(render_target, depth_buffer);
            context->ClearRenderTarget(render_target);
            context->ClearDepthStencil(depth_buffer);

            std::shared_ptr<gfx::RenderPipeline> curr_pipeline     = nullptr;
            std::shared_ptr<VertexArray>         curr_vertices     = nullptr;
            std::shared_ptr<IndexArray>          curr_indices      = nullptr;
            std::shared_ptr<MeshNode>            curr_instance     = nullptr;
            gfx::GpuBuffer*                      material_constant = nullptr;
            for (const auto& draw_call : draw_calls) {
                if (draw_call.material->GetPipeline() == nullptr) continue;

                if (curr_pipeline != draw_call.material->GetPipeline()) {
                    curr_pipeline = draw_call.material->GetPipeline();
                    context->SetPipeline(*curr_pipeline);
                    context->BindConstantBuffer(0, helper.Get<gfx::GpuBuffer>(data.frame_constant));
                    material_constant = &helper.Get<gfx::GpuBuffer>(data.material_constants.at(draw_call.material.get()));
                }
                if (curr_vertices != draw_call.vertices) {
                    curr_vertices = draw_call.vertices;
                    for (const auto& vertex_attr : curr_pipeline->desc.input_layout) {
                        auto attribute_data = curr_vertices->GetAttributeData(vertex_attr);
                        if (attribute_data.has_value()) {
                            context->SetVertexBuffer(vertex_attr.slot, *attribute_data->get().gpu_buffer);
                        }
                    }
                }
                if (curr_indices != draw_call.inidices) {
                    curr_indices = draw_call.inidices;
                    context->SetIndexBuffer(*curr_indices->GetIndexData().gpu_buffer);
                }
                if (curr_instance != draw_call.instance) {
                    curr_instance = draw_call.instance;
                    context->BindConstantBuffer(1, instance_constant, instance_constant_offset.at(curr_instance.get()));
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
}

}  // namespace hitagi::asset