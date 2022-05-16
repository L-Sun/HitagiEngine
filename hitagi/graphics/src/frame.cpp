#include "frame.hpp"
#include <hitagi/graphics/command_context.hpp>

#include <hitagi/graphics/driver_api.hpp>

#include <hitagi/utils/overloaded.hpp>
#include "hitagi/graphics/pipeline_state.hpp"
#include "spdlog/spdlog.h"

using namespace hitagi::math;

namespace hitagi::graphics {
Frame::Frame(DriverAPI& driver, ResourceManager& resource_manager, size_t frame_index, allocator_type alloc)
    : m_Driver(driver),
      m_ResMgr(resource_manager),
      m_FrameIndex(frame_index),
      m_Renderables(alloc),
      m_Output(m_Driver.CreateRenderFromSwapChain(frame_index)),
      m_ConstantBuffer(m_Driver.CreateConstantBuffer("Object Constant", 1, std::max({sizeof(FrameConstant), sizeof(ObjectConstant)}))) {}

void Frame::AddRenderables(std::pmr::vector<Renderable> renderables) {
    m_Renderables.insert(
        m_Renderables.end(),
        std::make_move_iterator(renderables.begin()),
        std::make_move_iterator(renderables.end()));
}

// void Frame::PrepareImGuiData(ImDrawData* data, std::shared_ptr<resource::Image> font_texture, const PipelineState& pso) {
//     // TODO auto resize constant buffer
//     // we need a element to store gui projection
//     if (m_ConstantBuffer->GetNumElements() - m_ConstantCount < 1)
//         m_Driver.ResizeConstantBuffer(m_ConstantBuffer, m_ConstantBuffer->GetNumElements() + 1);

//     m_GuiDrawInfo = std::make_shared<GuiDrawInformation>(
//         GuiDrawInformation{
//             .pipeline        = pso,
//             .font_texture    = m_ResMgr.GetTextureBuffer(font_texture),
//             .constant_offset = m_ConstantCount++,
//         });

//     const float left       = data->DisplayPos.x;
//     const float right      = data->DisplayPos.x + data->DisplaySize.x;
//     const float top        = data->DisplayPos.y;
//     const float bottom     = data->DisplayPos.y + data->DisplaySize.y;
//     const float near       = 3.0f;
//     const float far        = -1.0f;
//     const mat4f projection = ortho(left, right, bottom, top, near, far);
//     m_Driver.UpdateConstantBuffer(m_ConstantBuffer, m_GuiDrawInfo->constant_offset, reinterpret_cast<const uint8_t*>(&projection), sizeof(projection));

//     m_ResMgr.MakeImGuiMesh(
//         data,
//         [&](std::shared_ptr<hitagi::graphics::MeshBuffer> mesh, ImDrawList* curr_cmd_list, const ImDrawCmd& curr_cmd) {
//             vec2f clip_min(curr_cmd.ClipRect.x - data->DisplayPos.x, curr_cmd.ClipRect.y - data->DisplayPos.y);
//             vec2f clip_max(curr_cmd.ClipRect.z - data->DisplayPos.x, curr_cmd.ClipRect.w - data->DisplayPos.y);

//             std::array<uint32_t, 4> scissor_rect = {
//                 static_cast<uint32_t>(clip_min.x),
//                 static_cast<uint32_t>(clip_min.y),
//                 static_cast<uint32_t>(clip_max.x),
//                 static_cast<uint32_t>(clip_max.y),
//             };

//             m_GuiDrawInfo->scissor_rects.emplace_back(scissor_rect);
//             m_GuiDrawInfo->meshes.emplace_back(mesh);
//         });
// }

// void Frame::SetCamera(resource::CameraNode& camera) {
//     auto& data         = m_FrameConstant;
//     data.camera_pos    = vec4f(camera.GetCameraPosition(), 1.0f);
//     data.view          = camera.GetViewMatrix();
//     data.inv_view      = inverse(data.view);
//     auto camera_object = camera.GetSceneObjectRef().lock();
//     assert(camera_object != nullptr);
//     // TODO orth camera
//     data.projection = perspective(
//         camera_object->GetFov(),
//         camera_object->GetAspect(),
//         camera_object->GetNearClipDistance(),
//         camera_object->GetFarClipDistance());
//     data.inv_projection = inverse(data.projection);
//     data.proj_view      = data.projection * data.view;
//     data.inv_proj_view  = inverse(data.proj_view);

//     m_Driver.UpdateConstantBuffer(m_ConstantBuffer, 0, reinterpret_cast<uint8_t*>(&data), sizeof(data));
// }

// void Frame::SetLight(resource::LightNode& light) {
//     auto& data             = m_FrameConstant;
//     data.light_position    = light.GetCalculatedTransformation() * vec4f(1.0f);
//     data.light_pos_in_view = data.view * data.light_position;
//     if (auto light_obj = light.GetSceneObjectRef().lock()) {
//         data.light_intensity = light_obj->GetIntensity() * light_obj->GetColor();
//     }
//     m_Driver.UpdateConstantBuffer(m_ConstantBuffer, 0, reinterpret_cast<uint8_t*>(&data), sizeof(data));
// }

void Frame::Draw(IGraphicsCommandContext* context) {
    for (const auto& item : m_Renderables) {
        context->SetPipelineState(*m_ResMgr.GetPipelineState(item.material->GetGuid()));
        context->SetParameter("FrameConstant",
                              m_ConstantBuffer,
                              0);
        context->SetParameter("ObjectConstants",
                              m_ConstantBuffer,
                              item.object_constant_offset);
        context->SetParameter("MaterialConstants",
                              m_ResMgr.GetMaterialParameterBuffer(item.material->GetGuid()),
                              item.material_constant_offset);
        for (const auto& [name, texture] : item.material_instance->GetTextures()) {
            context->SetParameter(name, m_ResMgr.GetTextureBuffer(texture->GetGuid()));
        }

        context->Draw(m_ResMgr.GetVertexBuffer(item.vertices->GetGuid()),
                      m_ResMgr.GetIndexBuffer(item.indices->GetGuid()),
                      item.primitive);
    }
}

void Frame::ResetState() {
    m_Driver.WaitFence(m_FenceValue);
    m_Renderables.clear();
    m_ConstantCount = 1;
}

void Frame::SetRenderTarget(std::shared_ptr<RenderTarget> rt) {
    m_Driver.WaitFence(m_FenceValue);
    m_Output = std::move(rt);
}

// void Frame::PopulateMaterial(const resource::Material::Color& color, vec4f& value_dest, std::shared_ptr<TextureBuffer>& texture_dest) {
//     std::visit(utils::Overloaded{
//                    [&](const vec4f& value) {
//                        value_dest   = value;
//                        texture_dest = m_ResMgr.GetDefaultTextureBuffer(Format::R8G8B8A8_UNORM);
//                    },
//                    [&](std::shared_ptr<resource::Texture> texutre) {
//                        value_dest   = vec4f(-1.0f);
//                        texture_dest = m_ResMgr.GetTextureBuffer(texutre->GetTextureImage());
//                    }},
//                color);
// }

// void Frame::PopulateMaterial(const resource::Material::SingleValue& value, float& value_dest, std::shared_ptr<TextureBuffer>& texture_dest) {
//     std::visit(utils::Overloaded{
//                    [&](float value) {
//                        value_dest   = value;
//                        texture_dest = m_ResMgr.GetDefaultTextureBuffer(Format::R8_UNORM);
//                    },
//                    [&](std::shared_ptr<resource::Texture> texutre) {
//                        value_dest   = -1.0f;
//                        texture_dest = m_ResMgr.GetTextureBuffer(texutre->GetTextureImage());
//                    }},
//                value);
// }

void Frame::PrepareData() {
    // TODO auto resize constant buffer
    // if new size is smaller, the expand function return directly.
    if (m_ConstantBuffer->desc.num_elements < m_Renderables.size())
        // becase capacity + needed > exsisted + needed
        m_Driver.ResizeConstantBuffer(m_ConstantBuffer, m_Renderables.size());

    std::sort(
        m_Renderables.begin(),
        m_Renderables.end(),
        [](const Renderable& a, const Renderable& b) -> bool {
            return a.material < b.material;
        });

    std::size_t index = 0;
    for (auto& item : m_Renderables) {
        ObjectConstant constant{
            .word_transform   = item.transform->GetTransform(),
            .object_transform = item.transform->GetTransform(),
            .parent_transform = item.transform->GetParentTransform(),
            .translation      = vec4f(item.transform->GetTranslation(), 1.0f),
            .rotation         = item.transform->GetRotation(),
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
        m_ResMgr.PrepareMaterialParameterBuffer(material);
        if (auto material_buffer = m_ResMgr.GetMaterialParameterBuffer(material->GetGuid()); material_buffer) {
            auto& cpu_buffer = item.material_instance->GetParameterBuffer();

            m_Driver.UpdateConstantBuffer(
                material_buffer,
                index, cpu_buffer.GetData(),
                cpu_buffer.GetDataSize());

        } else if (material->GetNumInstances() * material->GetParametersSize() != 0) {
            spdlog::get("GraphicsManager")->warn(
                "Failed to get material constant buffer!"
                " Material:{}",
                material->GetUniqueName());
        } else {
            // That is ok, because there is not parameter used by material.
        }

        // Texture
        for (const auto& [name, texture] : item.material_instance->GetTextures()) {
            m_ResMgr.PrepareTextureBuffer(texture);
        }
        index++;
    }
}

}  // namespace hitagi::graphics