#include <hitagi/graphics/frame.hpp>
#include <hitagi/graphics/graphics_manager.hpp>
#include <hitagi/graphics/device_api.hpp>
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
    math::vec4f light_intensity;
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
    m_Device.InitRenderFromSwapChain(m_Output, frame_index);

    m_DepthBuffer = DepthBuffer{
        .format        = Format::D32_FLOAT,
        .width         = m_Output.width,
        .height        = m_Output.height,
        .clear_depth   = 1.0f,
        .clear_stencil = 0,
    };
    m_Device.InitDepthBuffer(m_DepthBuffer);

    m_FrameCB.name = "Frame Constant";
    m_ObjCB.name   = "Object Constant";
    m_DebugCB.name = "Debug Constant";

    m_Device.InitConstantBuffer(m_FrameCB);
    m_Device.InitConstantBuffer(m_ObjCB);
    m_Device.InitConstantBuffer(m_DebugCB);
}

void Frame::DrawScene(const resource::Scene& scene) {
    auto context = NewContext("SceneDraw");

    // grow constant buffer if need
    if (m_ObjCB.num_elements < scene.instance_nodes.size()) {
        m_ObjCB.Resize(m_ObjCB.num_elements + scene.instance_nodes.size());
    }

    std::pmr::unordered_map<const resource::Material*, std::size_t>         material_counter;
    std::pmr::unordered_map<const resource::MaterialInstance*, std::size_t> material_cb_index;
    for (const auto& material : scene.materials) {
        if (!m_MaterialBuffers.contains(material.get())) {
            auto [iter, success] = m_MaterialBuffers.emplace(
                material.get(),
                ConstantBuffer{
                    .num_elements = material->GetNumInstances(),
                    .element_size = material->GetParametersSize(),
                });
            iter->second.name = fmt::format("material cb ({})", material->name);
            m_Device.InitConstantBuffer(iter->second);
        } else if (m_MaterialBuffers.at(material.get()).num_elements < material->GetNumInstances()) {
            m_MaterialBuffers.at(material.get()).Resize(material->GetNumInstances());
        }
        material_counter.emplace(material.get(), 0);
    }

    const auto&   camera = scene.curr_camera->object;
    FrameConstant frame_constant{
        .camera_pos     = vec4f(camera->eye, 1.0f),
        .view           = camera->GetView(),
        .projection     = camera->GetProjection(),
        .proj_view      = camera->GetProjectionView(),
        .inv_view       = camera->GetInvView(),
        .inv_projection = camera->GetInvProjection(),
        .inv_proj_view  = camera->GetInvProjectionView(),
    };

    m_FrameCB.Update(0, frame_constant);

    auto view_port = camera->GetViewPort(m_Output.width, m_Output.height);
    context->SetViewPortAndScissor(view_port.x, view_port.y, view_port.z, view_port.w);
    context->SetRenderTargetAndDepthBuffer(m_Output, m_DepthBuffer);

    auto instances = scene.instance_nodes;
    std::sort(instances.begin(), instances.end(), [](const std::shared_ptr<MeshNode>& a, const std::shared_ptr<MeshNode>& b) {
        return a->object < b->object;
    });

    std::size_t instance_index = 0;
    for (const auto& node : instances) {
        auto mesh = node->object;

        context->UpdateVertexBuffer(*mesh->vertices);
        context->UpdateIndexBuffer(*mesh->indices);
        bool vertices_indices_bind = false;

        m_ObjCB.Update(instance_index, node->transform.world_matrix);

        for (const auto& submesh : mesh->sub_meshes) {
            auto material_instance = submesh.material_instance.get();
            if (!submesh.material_instance) return;
            auto material = submesh.material_instance->GetMaterial().lock();
            if (!material) continue;

            context->SetPipelineState(graphics_manager->GetPipelineState(material.get()));
            if (!vertices_indices_bind) {
                context->BindVertexBuffer(*mesh->vertices);
                context->BindIndexBuffer(*mesh->indices);
                vertices_indices_bind = true;
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

            context->DrawIndexed(submesh.index_count, submesh.index_offset, submesh.vertex_offset);
        }

        instance_index++;
    }
}

void Frame::DrawDebug(const DebugDrawData& debug_data) {
    if (!debug_data.mesh) return;
    auto context = NewContext("DebugDraw");

    context->UpdateVertexBuffer(*debug_data.mesh.vertices);
    context->UpdateIndexBuffer(*debug_data.mesh.indices);

    if (m_DebugCB.num_elements < debug_data.mesh.sub_meshes.size()) {
        m_DebugCB.Resize(m_DebugCB.num_elements + debug_data.mesh.sub_meshes.size());
    }
    for (std::size_t i = 0; i < debug_data.constants.size(); i++) {
        m_DebugCB.Update(i, debug_data.constants[i]);
    }

    context->SetPipelineState(graphics_manager->builtin_pipeline.debug);
    context->SetRenderTargetAndDepthBuffer(m_Output, m_DepthBuffer);
    context->BindVertexBuffer(*debug_data.mesh.vertices);
    context->BindIndexBuffer(*debug_data.mesh.indices);
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

    context->UpdateVertexBuffer(*gui_data.mesh.vertices);
    context->UpdateIndexBuffer(*gui_data.mesh.indices);

    if (gui_data.texture->gpu_resource == nullptr) {
        m_Device.InitTexture(*gui_data.texture);
    }

    context->SetRenderTarget(m_Output);
    context->SetPipelineState(graphics_manager->builtin_pipeline.gui);
    context->BindVertexBuffer(*gui_data.mesh.vertices);
    context->BindIndexBuffer(*gui_data.mesh.indices);
    context->BindDynamicConstantBuffer(0, reinterpret_cast<const std::byte*>(&gui_data.projection), sizeof(gui_data.projection));
    context->BindResource(0, *gui_data.texture);
    context->SetViewPort(gui_data.view_port.x, gui_data.view_port.y, gui_data.view_port.z, gui_data.view_port.w);

    for (std::size_t i = 0; i < gui_data.mesh.sub_meshes.size(); i++) {
        const auto& scissor = gui_data.scissor_rects.at(i);
        const auto& submesh = gui_data.mesh.sub_meshes.at(i);

        context->SetScissorRect(scissor.x, scissor.y, scissor.z, scissor.w);
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

    m_CommandContexts.clear();
}

void Frame::Wait() {
    m_Device.WaitFence(m_FenceValue);
}

void Frame::Reset() {
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
    m_Device.InitRenderFromSwapChain(m_Output, m_FrameIndex);
    m_DepthBuffer.width  = m_Output.width;
    m_DepthBuffer.height = m_Output.height;

    m_Device.InitDepthBuffer(m_DepthBuffer);
}

}  // namespace hitagi::graphics