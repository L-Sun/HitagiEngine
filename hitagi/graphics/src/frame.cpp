#include <hitagi/graphics/frame.hpp>
#include <hitagi/graphics/driver_api.hpp>
#include <hitagi/utils/overloaded.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>

using namespace hitagi::math;
using namespace hitagi::resource;

namespace hitagi::graphics {
Frame::Frame(DriverAPI& driver, ResourceManager& resource_manager, size_t frame_index)
    : m_Driver(driver),
      m_ResMgr(resource_manager),
      m_FrameIndex(frame_index),
      m_Output(m_Driver.CreateRenderFromSwapChain(frame_index)),
      m_ConstantBuffer(m_Driver.CreateConstantBuffer(
          "Object Constant",
          {.num_elements = 1,
           .element_size = std::max({sizeof(FrameConstant), sizeof(ObjectConstant)})})) {}

void Frame::AddRenderables(std::pmr::vector<Renderable> renderables) {
    m_Renderables.insert(
        m_Renderables.end(),
        std::make_move_iterator(renderables.begin()),
        std::make_move_iterator(renderables.end()));
}

void Frame::SetCamera(const std::shared_ptr<Camera>& camera) {
    auto& data      = m_FrameConstant;
    data.camera_pos = vec4f(camera->GetGlobalPosition(), 1.0f);
    data.view       = camera->GetViewMatrix();
    data.inv_view   = inverse(data.view);

    data.projection = perspective(
        camera->GetFov(),
        camera->GetAspect(),
        camera->GetNearClipDistance(),
        camera->GetFarClipDistance());
    data.inv_projection = inverse(data.projection);
    data.proj_view      = data.projection * data.view;
    data.inv_proj_view  = inverse(data.proj_view);

    m_Driver.UpdateConstantBuffer(m_ConstantBuffer, 0, reinterpret_cast<std::byte*>(&data), sizeof(data));
}

void Frame::Draw(IGraphicsCommandContext* context, Renderable::Type type) {
    for (const auto& item : m_Renderables) {
        if (item.type != type) continue;

        context->SetPipelineState(m_ResMgr.GetPipelineState(item.material->GetGuid()));
        context->SetParameter(FRAME_CONSTANT_BUFFER,
                              m_ConstantBuffer,
                              0);
        context->SetParameter(OBJECT_CONSTANT_BUFFER,
                              m_ConstantBuffer,
                              item.object_constant_offset);

        if (auto material_constant_buffer = m_ResMgr.GetMaterialParameterBuffer(item.material->GetGuid());
            material_constant_buffer) {
            context->SetParameter(MATERIAL_CONSTANT_BUFFER, material_constant_buffer, item.material_constant_offset);
        }

        for (const auto& [name, texture] : item.mesh.material->GetTextures()) {
            context->SetParameter(name, m_ResMgr.GetTextureBuffer(texture->GetGuid()));
        }

        if (item.pipeline_parameters.scissor_react) {
            const auto scissor_react = item.pipeline_parameters.scissor_react.value();
            context->SetScissorRect(scissor_react.x, scissor_react.y, scissor_react.z, scissor_react.w);
        }

        context->Draw(m_ResMgr.GetVertexBuffer(item.mesh.vertices->GetGuid()),
                      m_ResMgr.GetIndexBuffer(item.mesh.indices->GetGuid()),
                      item.material->GetPrimitiveType(),
                      item.mesh.index_count,
                      item.mesh.vertex_offset,
                      item.mesh.index_offset);
    }
}

void Frame::SetFenceValue(std::uint64_t fence_value) {
    m_FenceValue = fence_value;
    m_Lock       = true;
}

bool Frame::IsRenderingFinished() const {
    return m_Driver.IsFenceComplete(m_FenceValue);
}

void Frame::Reset() {
    m_Driver.WaitFence(m_FenceValue);
    m_Renderables.clear();
    m_ConstantCount = 1;
    m_Lock          = false;
}

void Frame::SetRenderTarget(std::shared_ptr<RenderTarget> rt) {
    m_Driver.WaitFence(m_FenceValue);
    m_Output = std::move(rt);
}

void Frame::PrepareData() {
    // TODO auto resize constant buffer
    // if new size is smaller, the expand function return directly.
    if (m_ConstantBuffer->desc.num_elements < m_Renderables.size() + 1)
        // becase capacity + needed > exsisted + needed
        m_Driver.ResizeConstantBuffer(m_ConstantBuffer, m_Renderables.size() + 1);

    std::stable_sort(
        m_Renderables.begin(),
        m_Renderables.end(),
        [](const Renderable& a, const Renderable& b) -> bool {
            if (a.material == b.material)
                return a.mesh.material < b.mesh.material;
            return a.material < b.material;
        });

    std::size_t                       index                  = 0;
    std::shared_ptr<Material>         last_material          = nullptr;
    std::shared_ptr<MaterialInstance> last_material_instance = nullptr;
    std::size_t                       material_offset        = 0;
    for (auto& item : m_Renderables) {
        m_ResMgr.PrepareVertexBuffer(item.mesh.vertices);
        m_ResMgr.PrepareIndexBuffer(item.mesh.indices);

        ObjectConstant constant{
            .word_transform   = item.transform->GetTransform(),
            .object_transform = item.transform->GetTransform(),
            .parent_transform = item.transform->GetParentTransform(),
            .translation      = vec4f(item.transform->GetTranslation(), 1.0f),
            .rotation         = rotate(mat4f(1.0f), item.transform->GetRotation()),
            .scaling          = vec4f(item.transform->GetScaling(), 1.0f),
        };
        // since we share the object constant value and frame constant value in the same constant buffer,
        // and the first element of constant buffer is frame constant value, so we offset the index one element
        m_Driver.UpdateConstantBuffer(m_ConstantBuffer,
                                      1 + index,
                                      reinterpret_cast<const std::byte*>(&constant),
                                      sizeof(constant));
        item.object_constant_offset = 1 + index;

        // Material parameter data
        auto material = item.material;
        m_ResMgr.PrepareMaterial(material);
        if (auto material_buffer = m_ResMgr.GetMaterialParameterBuffer(material->GetGuid()); material_buffer) {
            if (material != last_material || item.mesh.material != last_material_instance) {
                if (material != last_material)
                    material_offset = 0;
                else if (item.mesh.material != last_material_instance)
                    material_offset++;

                auto& cpu_buffer = item.mesh.material->GetParameterBuffer();
                m_Driver.UpdateConstantBuffer(
                    material_buffer,
                    material_offset,
                    cpu_buffer.GetData(),
                    cpu_buffer.GetDataSize());

                last_material          = material;
                last_material_instance = item.mesh.material;
            }
            item.material_constant_offset = material_offset;
        }

        // Texture
        for (const auto& [name, texture] : item.mesh.material->GetTextures()) {
            m_ResMgr.PrepareTextureBuffer(texture);
        }
        index++;
    }
}

}  // namespace hitagi::graphics