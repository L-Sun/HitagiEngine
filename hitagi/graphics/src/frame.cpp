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
Frame::Frame(DeviceAPI& device, std::size_t frame_index)
    : m_Device(device),
      m_FrameIndex(frame_index),
      m_ConstantBuffer{.num_elements = 1, .element_size = std::max({sizeof(FrameConstant), sizeof(ObjectConstant)})} {
    m_ConstantBuffer.name = "Frame and Object Constant";
    m_Device.InitRenderFromSwapChain(m_Output, frame_index);
    m_Device.InitConstantBuffer(m_ConstantBuffer);
}
void Frame::AppendRenderables(std::pmr::vector<resource::Renderable> renderables) {
    m_RenderItems.insert(m_RenderItems.end(), std::make_move_iterator(renderables.begin()), std::make_move_iterator(renderables.end()));
}

void Frame::SetCamera(resource::Camera camera) {
    m_FrameConstant.camera_pos     = vec4f(camera.eye, 1.0f);
    m_FrameConstant.view           = camera.GetView();
    m_FrameConstant.projection     = camera.GetProjectionView();
    m_FrameConstant.proj_view      = camera.GetInvProjectionView();
    m_FrameConstant.inv_view       = camera.GetInvView();
    m_FrameConstant.inv_projection = camera.GetInvProjection();
    m_FrameConstant.inv_proj_view  = camera.GetInvProjectionView();
    m_Device.UpdateConstantBuffer(m_ConstantBuffer, 0, reinterpret_cast<std::byte*>(&m_FrameConstant), sizeof(m_FrameConstant));
}

void Frame::Render(IGraphicsCommandContext* context, resource::Renderable::Type type) {
    assert(context);
    for (const auto& item : m_RenderItems) {
        if (item.type != type) continue;

        context->SetPipelineState(graphics_manager->GetPipelineState(item.material));
        context->BindResource(0, m_ConstantBuffer, 0);
        context->BindResource(1, m_ConstantBuffer, item.object_constant_offset);

        if (m_MaterialBuffers.count(item.material) != 0) {
            context->BindResource(2, m_MaterialBuffers.at(item.material), item.material_constant_offset);
        }

        std::uint32_t texture_slot = 0;
        for (const auto& [name, texture] : item.sub_mesh.material.GetTextures()) {
            context->BindResource(texture_slot++, *texture);
        }

        if (item.pipeline_parameters.scissor_react) {
            const auto scissor_react = item.pipeline_parameters.scissor_react.value();
            context->SetScissorRect(scissor_react.x, scissor_react.y, scissor_react.z, scissor_react.w);
        }
        if (item.pipeline_parameters.view_port) {
            const auto view_port = item.pipeline_parameters.view_port.value();
            context->SetViewPort(view_port.x, view_port.y, view_port.z, view_port.w);
        }

        context->Draw(*item.vertices, *item.indices,
                      item.material->primitive,
                      item.sub_mesh.index_count,
                      item.sub_mesh.vertex_offset,
                      item.sub_mesh.index_offset);
    }
}

void Frame::SetFenceValue(std::uint64_t fence_value) {
    m_FenceValue = fence_value;
    m_Lock       = true;
}

bool Frame::IsRenderingFinished() const {
    return m_Device.IsFenceComplete(m_FenceValue);
}

void Frame::Reset() {
    m_Device.WaitFence(m_FenceValue);
    m_RenderItems.clear();
    m_Lock = false;
}

void Frame::PrepareData() {
    auto context = m_Device.CreateGraphicsCommandContext();
    assert(context);

    std::stable_sort(m_RenderItems.begin(), m_RenderItems.end(), [](const Renderable& a, const Renderable& b) {
        return a.material < b.material;
    });

    // if new size is smaller, the expand function return directly.
    if (m_ConstantBuffer.num_elements < m_RenderItems.size() + 1)
        // becase capacity + needed > exsisted + needed
        m_Device.ResizeConstantBuffer(m_ConstantBuffer, m_RenderItems.size() + 1);

    // since we share the object constant value and frame constant value in the same constant buffer,
    // and the first element of constant buffer is frame constant value, so we offset the index one element
    std::size_t               object_constant_offset = 1;
    std::shared_ptr<Material> last_material          = nullptr;
    std::size_t               material_offset        = 0;

    for (auto& item : m_RenderItems) {
        // Prepare mesh buffer
        if (item.vertices->gpu_resource.lock() == nullptr) {
            m_Device.InitVertexBuffer(*item.vertices);
        }
        if (item.indices->gpu_resource.lock() == nullptr) {
            m_Device.InitIndexBuffer(*item.indices);
        }
        if (item.vertices->dirty) {
            context->UpdateVertexBuffer(*item.vertices);
            item.vertices->dirty = false;
        }
        if (item.indices->dirty) {
            context->UpdateIndexBuffer(*item.indices);
            item.indices->dirty = false;
        }

        // Prepare object constant buffer
        {
            ObjectConstant constant{
                .word_transform = item.transform.world_matrix,
            };

            m_Device.UpdateConstantBuffer(m_ConstantBuffer,
                                          object_constant_offset++,
                                          reinterpret_cast<const std::byte*>(&constant),
                                          sizeof(constant));
        }

        // Material constant buffer
        {
            auto material = item.material;
            if (material->GetParametersSize() != 0) {
                if (m_MaterialBuffers.count(material) == 0) {
                    ConstantBuffer constant_buffer{
                        .num_elements = material->GetNumInstances(),
                        .element_size = material->GetParametersSize(),
                    };
                    constant_buffer.name = material->name;
                    m_Device.InitConstantBuffer(constant_buffer);
                    m_MaterialBuffers[material] = std::move(constant_buffer);
                } else if (material->GetNumInstances() > m_MaterialBuffers.at(material).num_elements) {
                    m_Device.ResizeConstantBuffer(m_MaterialBuffers.at(material), material->GetNumInstances());
                }
            }

            if (m_MaterialBuffers.count(material) != 0) {
                auto& material_buffer = m_MaterialBuffers.at(material);

                // Since we sort rendere items by its material, we need reset the offset we processing new material
                if (material != last_material) {
                    material_offset = 0;
                }

                auto& cpu_buffer = item.sub_mesh.material.GetParameterBuffer();
                m_Device.UpdateConstantBuffer(
                    material_buffer,
                    material_offset,
                    cpu_buffer.GetData(),
                    cpu_buffer.GetDataSize());

                material_offset++;
                last_material = material;
            }
            item.material_constant_offset = material_offset;
        }

        // Prepare texture
        for (const auto& [name, texture] : item.sub_mesh.material.GetTextures()) {
            if (texture->gpu_resource.lock() == nullptr) {
                m_Device.InitTexture(*texture);
            }
        }
    }
    context->Finish(true);
}
}  // namespace hitagi::graphics