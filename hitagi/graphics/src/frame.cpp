#include <hitagi/graphics/frame.hpp>
#include <hitagi/graphics/graphics_manager.hpp>
#include <hitagi/graphics/device_api.hpp>
#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/utils/overloaded.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <iterator>

using namespace hitagi::math;
using namespace hitagi::resource;

namespace hitagi::graphics {
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

struct ObjectConstant {
    math::mat4f word_transform;
};

Frame::Frame(DeviceAPI& device, std::size_t frame_index)
    : m_Device(device),
      m_FrameIndex(frame_index),
      m_FrameCB{.num_elements = 1, .element_size = sizeof(FrameConstant)},
      m_ObjCB{.num_elements = 100, .element_size = sizeof(ObjectConstant)},
      m_DebugCB{.num_elements = 100, .element_size = /*transform*/ sizeof(mat4f) + /*color*/ sizeof(vec4f)} {
    m_Device.InitRenderTargetFromSwapChain(m_Output, frame_index);

    m_DepthBuffer = resource::Texture{
        .bind_flags = resource::Texture::BindFlag::DepthBuffer,
        .format     = Format::D32_FLOAT,
        .width      = m_Output.width,
        .height     = m_Output.height,
    };
    m_DepthBuffer.clear_value.depth_stencil.depth   = 1.0f;
    m_DepthBuffer.clear_value.depth_stencil.stencil = 0;

    m_Device.InitTexture(m_DepthBuffer);

    m_FrameCB.name = "Frame Constant";
    m_ObjCB.name   = "Object Constant";
    m_DebugCB.name = "Debug Constant";

    m_Device.InitConstantBuffer(m_FrameCB);
    m_Device.InitConstantBuffer(m_ObjCB);
    m_Device.InitConstantBuffer(m_DebugCB);
}

void Frame::DrawScene(const resource::Scene& scene, const std::shared_ptr<resource::Texture>& render_texture) {
    auto context = NewContext("SceneDraw");

    std::shared_ptr<resource::Texture> p_depth_buffer = nullptr;
    if (render_texture != nullptr) {
        if (render_texture->dirty) {
            m_Device.RetireResource(std::move(render_texture->gpu_resource));
            m_Device.InitTexture(*render_texture);
        }
        context->ClearRenderTarget(*render_texture);
        m_TempResources.emplace_back(render_texture);

        p_depth_buffer                                    = std::make_shared<resource::Texture>();
        p_depth_buffer->bind_flags                        = resource::Texture::BindFlag::DepthBuffer,
        p_depth_buffer->format                            = Format::D32_FLOAT,
        p_depth_buffer->width                             = render_texture->width,
        p_depth_buffer->height                            = render_texture->height,
        p_depth_buffer->clear_value.depth_stencil.depth   = 1.0f;
        p_depth_buffer->clear_value.depth_stencil.stencil = 0;
        m_Device.InitTexture(*p_depth_buffer);

        context->ClearDepthBuffer(*p_depth_buffer);
        m_TempResources.emplace_back(p_depth_buffer);
    }
    resource::Texture& render_target = render_texture ? *render_texture : m_Output;
    resource::Texture& depth_buffer  = render_texture ? *p_depth_buffer : m_DepthBuffer;

    // grow constant buffer if need
    if (m_ObjCB.num_elements < scene.instance_nodes.size()) {
        m_ObjCB.Resize(m_ObjCB.num_elements + scene.instance_nodes.size());
    }

    std::pmr::unordered_map<const resource::Material*, std::size_t>         material_counter;
    std::pmr::unordered_map<const resource::MaterialInstance*, std::size_t> material_cb_index;
    for (const auto& material : asset_manager->GetAllMaterials()) {
        if (!m_MaterialBuffers.contains(material.get()) && material->GetNumInstances() != 0 && material->GetParametersBufferSize() != 0) {
            auto [iter, success] = m_MaterialBuffers.emplace(
                material.get(),
                ConstantBuffer{
                    .num_elements = material->GetNumInstances(),
                    .element_size = material->GetParametersBufferSize(),
                });
            iter->second.name = fmt::format("material cb ({})", material->GetName());
            m_Device.InitConstantBuffer(iter->second);
        }
        if (m_MaterialBuffers.contains(material.get()) && m_MaterialBuffers.at(material.get()).num_elements < material->GetNumInstances()) {
            m_MaterialBuffers.at(material.get()).Resize(material->GetNumInstances());
        }
        material_counter.emplace(material.get(), 0);
    }

    // TODO multiple light
    const auto&   camera     = scene.curr_camera->object;
    const auto&   light_node = scene.light_nodes.front();
    const auto&   light      = light_node->object;
    FrameConstant frame_constant{
        .camera_pos        = vec4f(camera->eye, 1.0f),
        .view              = camera->GetView(),
        .projection        = camera->GetProjection(),
        .proj_view         = camera->GetProjectionView(),
        .inv_view          = camera->GetInvView(),
        .inv_projection    = camera->GetInvProjection(),
        .inv_proj_view     = camera->GetInvProjectionView(),
        .light_position    = vec4f(light_node->transform.GetPosition(), 1.0f),
        .light_pos_in_view = camera->GetView() * vec4f(light_node->transform.GetPosition(), 1.0f),
        .light_color       = light->color,
        .light_intensity   = light->intensity,
    };

    m_FrameCB.Update(0, frame_constant);

    auto view_port = camera->GetViewPort(render_target.width, render_target.height);
    context->SetViewPortAndScissor(view_port.x, view_port.y, view_port.z, view_port.w);
    context->SetRenderTargetAndDepthBuffer(render_target, depth_buffer);

    auto instances = scene.instance_nodes;
    std::sort(instances.begin(), instances.end(), [](const std::shared_ptr<MeshNode>& a, const std::shared_ptr<MeshNode>& b) {
        return a->object < b->object;
    });

    std::size_t instance_index = 0;
    for (const auto& node : instances) {
        auto mesh = node->object;
        if (mesh == nullptr || !mesh) continue;

        for (const auto& attribute_array : mesh->vertices) {
            if (attribute_array) context->UpdateVertexBuffer(*attribute_array);
        }
        context->UpdateIndexBuffer(*mesh->indices);
        bool mesh_bind = false;

        m_ObjCB.Update(instance_index, node->transform.world_matrix);

        for (const auto& submesh : mesh->sub_meshes) {
            auto material_instance = submesh.material_instance.get();
            if (!submesh.material_instance) return;
            auto material = submesh.material_instance->GetMaterial();
            if (!material) continue;

            context->SetPipelineState(graphics_manager->GetPipelineState(material.get()));
            if (!mesh_bind) {
                context->BindMeshBuffer(*mesh);
            }

            context->BindResource(0, m_FrameCB, 0);
            context->BindResource(1, m_ObjCB, instance_index);

            if (const auto& material_buffer = material_instance->GetParameterBuffer(); !material_buffer.Empty()) {
                auto& material_cb = m_MaterialBuffers.at(material.get());

                if (!material_cb_index.contains(material_instance)) {
                    auto& counter = material_counter.at(material.get());
                    material_cb_index.emplace(material_instance, counter);
                    material_cb.Update(counter, material_buffer.GetData(), material_buffer.GetDataSize());
                    counter++;
                }

                context->BindResource(2, material_cb, material_cb_index.at(material_instance));
            }

            // Bind texture
            {
                std::size_t slot = 0;
                for (const auto& texture : material_instance->GetTextures()) {
                    if (texture) context->BindResource(slot++, *texture);
                }
            }

            context->DrawIndexed(submesh.index_count, submesh.index_offset, submesh.vertex_offset);
        }

        instance_index++;
    }
}

void Frame::DrawDebug(const DebugDrawData& debug_data) {
    if (!debug_data.mesh) return;
    auto context = NewContext("DebugDraw");

    for (const auto& attribute_array : debug_data.mesh.vertices) {
        if (attribute_array) context->UpdateVertexBuffer(*attribute_array);
    }
    context->UpdateIndexBuffer(*debug_data.mesh.indices);

    if (m_DebugCB.num_elements < debug_data.mesh.sub_meshes.size()) {
        m_DebugCB.Resize(m_DebugCB.num_elements + debug_data.mesh.sub_meshes.size());
    }
    for (std::size_t i = 0; i < debug_data.constants.size(); i++) {
        m_DebugCB.Update(i, debug_data.constants[i]);
    }

    context->SetPipelineState(graphics_manager->builtin_pipeline.debug);
    context->SetRenderTargetAndDepthBuffer(m_Output, m_DepthBuffer);
    context->BindMeshBuffer(debug_data.mesh);
    context->BindDynamicConstantBuffer(0, reinterpret_cast<const std::byte*>(&debug_data.project_view), sizeof(debug_data.project_view));
    context->SetViewPortAndScissor(debug_data.view_port.x, debug_data.view_port.y, debug_data.view_port.z, debug_data.view_port.w);

    std::size_t index = 0;
    for (const auto& sub_mesh : debug_data.mesh.sub_meshes) {
        context->BindResource(1, m_DebugCB, index++);
        context->DrawIndexed(sub_mesh.index_count, sub_mesh.index_offset, sub_mesh.vertex_offset);
    }
}

void Frame::DrawGUI(const GuiDrawData& gui_data) {
    if (!gui_data.mesh) return;

    auto context = NewContext("GuiDraw");

    for (const auto& attribute_array : gui_data.mesh.vertices) {
        if (attribute_array) context->UpdateVertexBuffer(*attribute_array);
    }
    context->UpdateIndexBuffer(*gui_data.mesh.indices);

    for (auto texture : gui_data.textures) {
        if (texture && texture->gpu_resource == nullptr) {
            m_Device.InitTexture(*texture);
        }
    }

    context->SetRenderTarget(m_Output);
    context->SetPipelineState(graphics_manager->builtin_pipeline.gui);
    context->BindMeshBuffer(gui_data.mesh);
    context->BindDynamicConstantBuffer(0, reinterpret_cast<const std::byte*>(&gui_data.projection), sizeof(gui_data.projection));
    context->SetViewPort(gui_data.view_port.x, gui_data.view_port.y, gui_data.view_port.z, gui_data.view_port.w);

    for (std::size_t i = 0; i < gui_data.mesh.sub_meshes.size(); i++) {
        const auto& scissor = gui_data.scissor_rects.at(i);
        const auto& submesh = gui_data.mesh.sub_meshes.at(i);

        context->SetScissorRect(scissor.x, scissor.y, scissor.z, scissor.w);
        if (gui_data.textures[i]) context->BindResource(0, *gui_data.textures[i]);
        context->DrawIndexed(submesh.index_count, submesh.index_offset, submesh.vertex_offset);
    }
}

IGraphicsCommandContext* Frame::NewContext(std::string_view name) {
    return m_CommandContexts.emplace_back(m_Device.CreateGraphicsCommandContext(name)).get();
}

void Frame::Execute() {
    auto context = NewContext("Present");
    context->Present(m_Output);

    for (auto& context : m_CommandContexts) {
        m_FenceValue = std::max(m_FenceValue, context->Finish());
    }

    m_Output.gpu_resource->fence_value      = m_FenceValue;
    m_DepthBuffer.gpu_resource->fence_value = m_FenceValue;
    for (const auto& temp_res : m_TempResources) {
        temp_res->gpu_resource->fence_value = m_FenceValue;
    }

    m_CommandContexts.clear();
}

void Frame::Wait() {
    m_Device.WaitFence(m_FenceValue);
}

void Frame::Reset() {
    m_TempResources.clear();

    auto context = NewContext("Clear RT and DB");

    context->ClearRenderTarget(m_Output);
    context->ClearDepthBuffer(m_DepthBuffer);
}

void Frame::BeforeSwapchainSizeChanged() {
    m_CommandContexts.clear();
    Wait();
    m_Device.RetireResource(std::move(m_Output.gpu_resource));
    m_Device.RetireResource(std::move(m_DepthBuffer.gpu_resource));
}

void Frame::AfterSwapchainSizeChanged() {
    m_Device.InitRenderTargetFromSwapChain(m_Output, m_FrameIndex);
    m_DepthBuffer.width  = m_Output.width;
    m_DepthBuffer.height = m_Output.height;

    m_Device.InitTexture(m_DepthBuffer);
}

}  // namespace hitagi::graphics