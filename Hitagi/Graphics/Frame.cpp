#include "Frame.hpp"
#include "DriverAPI.hpp"
#include "ICommandContext.hpp"
#include "Utils.hpp"

using namespace Hitagi::Math;

namespace Hitagi::Graphics {
Frame::Frame(DriverAPI& driver, ResourceManager& resource_manager, size_t frame_index)
    : m_Driver(driver),
      m_ResMgr(resource_manager),
      m_FrameIndex(frame_index),
      m_ConstantBuffer(m_Driver.CreateConstantBuffer("Object Constant", 1, std::max({sizeof(FrameConstant), sizeof(ObjectConstant)}))),
      m_Output(m_Driver.CreateRenderFromSwapChain(frame_index)) {
}

void Frame::AddGeometries(const std::vector<std::shared_ptr<Asset::GeometryNode>>& geometries, const PipelineState& pso) {
    // Calculate need constant buffer size
    size_t constant_count = geometries.size();

    // TODO auto resize constant buffer
    // if new size is smaller, the expand function return directly.
    if (m_ConstantBuffer->GetNumElements() - m_ConstantCount < constant_count)
        // becase capacity + needed > exsisted + needed
        m_Driver.ResizeConstantBuffer(m_ConstantBuffer, m_ConstantBuffer->GetNumElements() + constant_count);

    for (auto node : geometries) {
        if (auto geometry = node->GetSceneObjectRef().lock()) {
            // need update
            ObjectConstant data;
            data.transform = node->GetCalculatedTransformation();

            for (auto&& mesh : geometry->GetMeshes()) {
                DrawItem item{
                    .pipeline        = pso,
                    .constant_offset = m_ConstantCount,
                    .mesh            = m_ResMgr.GetMeshBuffer(mesh),
                };

                // Updata Material data
                if (auto material = mesh.GetMaterial().lock()) {
                    PopulateMaterial(material->GetAmbientColor(), data.ambient, item.ambient);
                    PopulateMaterial(material->GetDiffuseColor(), data.diffuse, item.diffuse);
                    PopulateMaterial(material->GetEmission(), data.emission, item.emission);
                    PopulateMaterial(material->GetSpecularColor(), data.specular, item.specular);
                    PopulateMaterial(material->GetSpecularPower(), data.specular_power, item.specular_power);
                } else {
                    item.ambient        = m_ResMgr.GetDefaultTextureBuffer(Format::R8G8B8A8_UNORM);
                    item.diffuse        = m_ResMgr.GetDefaultTextureBuffer(Format::R8G8B8A8_UNORM);
                    item.emission       = m_ResMgr.GetDefaultTextureBuffer(Format::R8G8B8A8_UNORM);
                    item.specular       = m_ResMgr.GetDefaultTextureBuffer(Format::R8G8B8A8_UNORM);
                    item.specular_power = m_ResMgr.GetDefaultTextureBuffer(Format::R8_UNORM);
                }

                m_Driver.UpdateConstantBuffer(m_ConstantBuffer, m_ConstantCount, reinterpret_cast<const uint8_t*>(&data), sizeof(data));
                m_DrawItems.emplace_back(std::move(item));
                m_ConstantCount++;
            }
        }
    }
}

void Frame::AddDebugPrimitives(const std::vector<Debugger::DebugPrimitive>& primitives, const PipelineState& pso) {
    // TODO auto resize constant buffer
    // if new size is smaller, the expand function return directly.
    if (m_ConstantBuffer->GetNumElements() - m_ConstantCount < primitives.size())
        // becase capacity + needed > exsisted + needed
        m_Driver.ResizeConstantBuffer(m_ConstantBuffer, m_ConstantBuffer->GetNumElements() + primitives.size());

    for (auto&& primitive : primitives) {
        if (auto geometry = primitive.geometry_node->GetSceneObjectRef().lock()) {
            for (auto&& mesh : geometry->GetMeshes()) {
                DebugDrawItem item{
                    .pipeline        = pso,
                    .mesh            = m_ResMgr.GetMeshBuffer(mesh),
                    .constant_offset = m_ConstantCount};

                // TODO
                struct {
                    mat4f transform;
                    vec4f color;
                } data;
                data.transform = primitive.geometry_node->GetCalculatedTransformation();
                data.color     = primitive.color;
                m_Driver.UpdateConstantBuffer(m_ConstantBuffer, m_ConstantCount++, reinterpret_cast<const uint8_t*>(&data), sizeof(data));

                m_DebugItems.emplace_back(std::move(item));
            }
        }
    }
}

void Frame::PrepareImGuiData(ImDrawData* data, std::shared_ptr<Asset::Image> font_texture, const PipelineState& pso) {
    // TODO auto resize constant buffer
    // we need a element to store gui projection
    if (m_ConstantBuffer->GetNumElements() - m_ConstantCount < 1)
        m_Driver.ResizeConstantBuffer(m_ConstantBuffer, m_ConstantBuffer->GetNumElements() + 1);

    m_GuiDrawInfo = std::make_shared<GuiDrawInformation>(
        GuiDrawInformation{
            .pipeline        = pso,
            .font_texture    = m_ResMgr.GetTextureBuffer(font_texture),
            .constant_offset = m_ConstantCount++,
        });

    const float left       = data->DisplayPos.x;
    const float right      = data->DisplayPos.x + data->DisplaySize.x;
    const float top        = data->DisplayPos.y;
    const float bottom     = data->DisplayPos.y + data->DisplaySize.y;
    const float near       = 3.0f;
    const float far        = -1.0f;
    const mat4f projection = ortho(left, right, bottom, top, near, far);
    m_Driver.UpdateConstantBuffer(m_ConstantBuffer, m_GuiDrawInfo->constant_offset, reinterpret_cast<const uint8_t*>(&projection), sizeof(projection));

    m_ResMgr.MakeImGuiMesh(
        data,
        [&](std::shared_ptr<Hitagi::Graphics::MeshBuffer> mesh, ImDrawList* curr_cmd_list, const ImDrawCmd& curr_cmd) {
            vec2f clip_min(curr_cmd.ClipRect.x - data->DisplayPos.x, curr_cmd.ClipRect.y - data->DisplayPos.y);
            vec2f clip_max(curr_cmd.ClipRect.z - data->DisplayPos.x, curr_cmd.ClipRect.w - data->DisplayPos.y);

            std::array<uint32_t, 4> scissor_rect = {
                static_cast<uint32_t>(clip_min.x),
                static_cast<uint32_t>(clip_min.y),
                static_cast<uint32_t>(clip_max.x),
                static_cast<uint32_t>(clip_max.y),
            };

            m_GuiDrawInfo->scissor_rects.emplace_back(scissor_rect);
            m_GuiDrawInfo->meshes.emplace_back(mesh);
        });
}

void Frame::SetCamera(Asset::CameraNode& camera) {
    auto& data         = m_FrameConstant;
    data.camera_pos    = vec4f(camera.GetCameraPosition(), 1.0f);
    data.view          = camera.GetViewMatrix();
    data.inv_view      = inverse(data.view);
    auto camera_object = camera.GetSceneObjectRef().lock();
    assert(camera_object != nullptr);
    // TODO orth camera
    data.projection = perspective(
        camera_object->GetFov(),
        camera_object->GetAspect(),
        camera_object->GetNearClipDistance(),
        camera_object->GetFarClipDistance());
    data.inv_projection = inverse(data.projection);
    data.proj_view      = data.projection * data.view;
    data.inv_proj_view  = inverse(data.proj_view);

    m_Driver.UpdateConstantBuffer(m_ConstantBuffer, 0, reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

void Frame::SetLight(Asset::LightNode& light) {
    auto& data             = m_FrameConstant;
    data.light_position    = light.GetCalculatedTransformation() * vec4f(1.0f);
    data.light_pos_in_view = data.view * data.light_position;
    if (auto light_obj = light.GetSceneObjectRef().lock()) {
        data.light_intensity = light_obj->GetIntensity() * light_obj->GetColor();
    }
    m_Driver.UpdateConstantBuffer(m_ConstantBuffer, 0, reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

void Frame::Draw(IGraphicsCommandContext* context) {
    for (const auto& item : m_DrawItems) {
        context->SetPipelineState(item.pipeline);
        context->SetParameter("FrameConstant", *m_ConstantBuffer, 0);
        context->SetParameter("ObjectConstants", *m_ConstantBuffer, item.constant_offset);
        context->SetParameter("BaseSampler", *m_ResMgr.GetSampler("BaseSampler"));
        context->SetParameter("AmbientTexture", *item.ambient);
        context->SetParameter("DiffuseTexture", *item.diffuse);
        context->SetParameter("EmissionTexture", *item.emission);
        context->SetParameter("SpecularTexture", *item.specular);
        context->SetParameter("PowerTexture", *item.specular_power);
        context->Draw(*item.mesh);
    }
}
void Frame::GuiDraw(IGraphicsCommandContext* context) {
    context->SetPipelineState(m_GuiDrawInfo->pipeline);
    context->SetParameter("Constant", *m_ConstantBuffer, m_GuiDrawInfo->constant_offset);
    context->SetParameter("Texture", *m_GuiDrawInfo->font_texture);

    for (size_t i = 0; i < m_GuiDrawInfo->meshes.size(); i++) {
        auto scissor_rects = m_GuiDrawInfo->scissor_rects[i];
        context->SetScissorRect(scissor_rects[0], scissor_rects[1], scissor_rects[2], scissor_rects[3]);
        context->Draw(*m_GuiDrawInfo->meshes[i]);
    }
}

void Frame::DebugDraw(IGraphicsCommandContext* context) {
    for (const auto& item : m_DebugItems) {
        context->SetPipelineState(item.pipeline);
        context->SetParameter("FrameConstant", *m_ConstantBuffer, 0);
        context->SetParameter("ObjectConstants", *m_ConstantBuffer, item.constant_offset);
        context->Draw(*item.mesh);
    }
}

void Frame::ResetState() {
    m_Driver.WaitFence(m_FenceValue);
    m_DrawItems.clear();
    m_GuiDrawInfo = nullptr;
    m_DebugItems.clear();
    m_ConstantCount = 1;
}

void Frame::SetRenderTarget(std::shared_ptr<RenderTarget> rt) {
    m_Driver.WaitFence(m_FenceValue);
    m_Output = rt;
}

void Frame::PopulateMaterial(const Asset::Material::Color& color, vec4f& value_dest, std::shared_ptr<TextureBuffer>& texture_dest) {
    std::visit(Overloaded{
                   [&](const vec4f& value) {
                       value_dest   = value;
                       texture_dest = m_ResMgr.GetDefaultTextureBuffer(Format::R8G8B8A8_UNORM);
                   },
                   [&](std::shared_ptr<Asset::Texture> texutre) {
                       value_dest   = vec4f(-1.0f);
                       texture_dest = m_ResMgr.GetTextureBuffer(texutre->GetTextureImage());
                   }},
               color);
}

void Frame::PopulateMaterial(const Asset::Material::SingleValue& value, float& value_dest, std::shared_ptr<TextureBuffer>& texture_dest) {
    std::visit(Overloaded{
                   [&](float value) {
                       value_dest   = value;
                       texture_dest = m_ResMgr.GetDefaultTextureBuffer(Format::R8_UNORM);
                   },
                   [&](std::shared_ptr<Asset::Texture> texutre) {
                       value_dest   = -1.0f;
                       texture_dest = m_ResMgr.GetTextureBuffer(texutre->GetTextureImage());
                   }},
               value);
}

}  // namespace Hitagi::Graphics