#include "scene_viewport.hpp"
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/gfx/graphics_manager.hpp>
#include <hitagi/asset/asset_manager.hpp>

using namespace hitagi;
using namespace hitagi::math;
using namespace hitagi::asset;

struct FrameConstant {
    vec4f camera_pos;
    mat4f view;
    mat4f projection;
    mat4f proj_view;
    mat4f inv_view;
    mat4f inv_projection;
    mat4f inv_proj_view;
    vec4f light_position;
    vec4f light_pos_in_view;
    vec3f light_color;
    float light_intensity;
};

struct InstanceConstant {
    mat4f model;
};

bool SceneViewPort::Initialize() {
    return RuntimeModule::Initialize();
}

void SceneViewPort::Tick() {
    struct SceneRenderPass {
        gfx::ResourceHandle                                                     frame_constant;
        gfx::ResourceHandle                                                     instance_constant;
        std::pmr::unordered_map<std::shared_ptr<Material>, gfx::ResourceHandle> material_constant;
        gfx::ResourceHandle                                                     depth_buffer;
        gfx::ResourceHandle                                                     output;
    };

    gui_manager->DrawGui([&]() {
        if (ImGui::Begin("Scene Viewer", &m_Open)) {
            if (m_CurrentScene) {
                auto window_size = ImGui::GetWindowSize();

                auto pass = graphics_manager->GetRenderGraph().AddPass<SceneRenderPass>(
                    "SceneViewPortRenderPass",
                    [&](gfx::RenderGraph::Builder& builder, SceneRenderPass& data) {
                        data.frame_constant = builder.Create(gfx::GpuBuffer::Desc{
                            .name         = "frame-constant",
                            .element_size = sizeof(FrameConstant),
                            .usages       = gfx::GpuBuffer::UsageFlags::MapWrite | gfx::GpuBuffer::UsageFlags::Constant,
                        });

                        data.instance_constant = builder.Create(gfx::GpuBuffer::Desc{
                            .name          = "instant-constant",
                            .element_size  = sizeof(InstanceConstant),
                            .element_count = m_CurrentScene->instance_nodes.size(),
                            .usages        = gfx::GpuBuffer::UsageFlags::MapWrite | gfx::GpuBuffer::UsageFlags::Constant,
                        });

                        for (const auto& material : asset_manager->GetAllMaterials()) {
                            data.material_constant[material] = builder.Create(gfx::GpuBuffer::Desc{
                                .name          = material->GetName(),
                                .element_size  = material->CalculateMaterialBufferSize(),
                                .element_count = material->GetNumInstances(),
                                .usages        = gfx::GpuBuffer::UsageFlags::MapWrite | gfx::GpuBuffer::UsageFlags::Constant,
                            });
                        }

                        data.output = builder.Create(gfx::Texture::Desc{
                            .name   = "scene-output",
                            .width  = static_cast<std::uint32_t>(window_size.x),
                            .height = static_cast<std::uint32_t>(window_size.x),
                            .format = gfx::Format::R8G8B8A8_UNORM,
                            .usages = gfx::Texture::UsageFlags::SRV | gfx::Texture::UsageFlags::RTV,
                        });

                        data.depth_buffer = builder.Create(gfx::Texture::Desc{
                            .name   = "scene-depth",
                            .width  = static_cast<std::uint32_t>(window_size.x),
                            .height = static_cast<std::uint32_t>(window_size.x),
                            .format = gfx::Format::D32_FLOAT,
                            .usages = gfx::Texture::UsageFlags::DSV,
                        });
                    },
                    [=](const gfx::RenderGraph::ResourceHelper& helper, const SceneRenderPass& data, gfx::GraphicsCommandContext* context) {
                        // TODO draw scene
                        auto& frame_constant = helper.Get<gfx::GpuBuffer>(data.frame_constant);

                        const auto& camera = m_CurrentScene->curr_camera;
                        const auto& light  = m_CurrentScene->light_nodes.front();
                        frame_constant.Update(
                            0,
                            FrameConstant{
                                .camera_pos     = vec4f(camera->transform.GetPosition(), 1.0f),
                                .view           = camera->GetView(),
                                .projection     = camera->GetProjection(),
                                .proj_view      = camera->GetProjectionView(),
                                .inv_view       = camera->GetInvView(),
                                .inv_projection = camera->GetInvProjection(),
                                .inv_proj_view  = camera->GetInvProjectionView(),
                                // multiple light,
                                .light_position    = vec4f(light->GetLightGlobalPosition(), 1.0f),
                                .light_pos_in_view = camera->GetView() * vec4f(light->GetLightGlobalPosition(), 1.0f),
                                .light_color       = light->GetObjectRef()->parameters.color,
                                .light_intensity   = light->GetObjectRef()->parameters.intensity,
                            });

                        auto& instance_constants = helper.Get<gfx::GpuBuffer>(data.instance_constant);

                        std::pmr::unordered_map<const Material*, std::size_t> material_couter;
                        for (const auto& [material, handle] : data.material_constant) {
                            material_couter[material.get()] = 0;
                        }

                        std::shared_ptr<gfx::RenderPipeline> curr_pipeline = nullptr;

                        for (std::size_t instance_index = 0; instance_index < m_CurrentScene->instance_nodes.size(); instance_index++) {
                            const auto& node = m_CurrentScene->instance_nodes[instance_index];

                            instance_constants.Update(instance_index, node->transform.world_matrix);

                            auto mesh = node->GetObjectRef();
                            for (const auto& submesh : mesh->GetSubMeshes()) {
                                auto material = submesh.material_instance->GetMaterial();
                                if (material->GetPipeline() == nullptr) {
                                    material->InitPipeline(context->device);
                                }

                                if (curr_pipeline != material->GetPipeline()) {
                                    curr_pipeline = material->GetPipeline();
                                    context->SetPipeline(*curr_pipeline);
                                    context->BindConstantBuffer(0, frame_constant);
                                    context->BindConstantBuffer(1, instance_constants, instance_index);
                                }

                                auto  material_buffer   = submesh.material_instance->GetMateriaBufferData();
                                auto& material_constant = helper.Get<gfx::GpuBuffer>(data.material_constant.at(material));
                                material_constant.Update(material_couter[material.get()], material_buffer.Span<const std::byte>());
                                context->BindConstantBuffer(2, material_constant, material_couter[material.get()]++);

                                std::size_t tex_index = 0;
                                for (const auto& texture : submesh.material_instance->GetTextures()) {
                                    if (texture->GetGpuData() == nullptr) {
                                        texture->InitGpuData(context->device);
                                    }
                                    context->BindTexture(tex_index++, *texture->GetGpuData());
                                }
                            }
                        }
                    });

                ImGui::Image((void*)gui_manager->ReadTexture(pass.output).id, window_size);
            }
        }
        ImGui::End();
    });

    RuntimeModule::Tick();
}
