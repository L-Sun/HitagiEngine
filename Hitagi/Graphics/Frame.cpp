#include "Frame.hpp"
#include "DriverAPI.hpp"
#include "ICommandContext.hpp"

namespace Hitagi::Graphics {
Frame::Frame(DriverAPI& driver, ResourceManager& resource_manager, size_t frame_index)
    : m_Driver(driver),
      m_ResMgr(resource_manager),
      m_FrameIndex(frame_index),
      m_ConstantBuffer(m_Driver.CreateConstantBuffer("Object Constant", 1, std::max({sizeof(FrameConstant), sizeof(ObjectConstant)}))),
      m_Output(m_Driver.CreateRenderFromSwapChain(frame_index)) {
}

void Frame::AddGeometries(std::vector<std::reference_wrapper<Asset::SceneGeometryNode>> geometries, const PipelineState& pso) {
    // Calculate need constant buffer size
    size_t constant_count = geometries.size();
    for (Asset::SceneGeometryNode& node : geometries) {
        if (auto geometry = node.GetSceneObjectRef().lock()) {
            constant_count += geometry->GetMeshes().size();
        }
    }

    // TODO auto resize constant buffer
    // if new size is smaller, the expand function return directly.
    if (m_ConstantBuffer->GetNumElements() - m_ConstantCount < constant_count)
        // becase capacity + needed > exsisted + needed
        m_Driver.ResizeConstantBuffer(m_ConstantBuffer, m_ConstantBuffer->GetNumElements() + constant_count);

    for (Asset::SceneGeometryNode& node : geometries) {
        if (auto geometry = node.GetSceneObjectRef().lock()) {
            // need update
            ObjectConstant data;
            data.transform = node.GetCalculatedTransform();

            auto& meshes = geometry->GetMeshes();
            // Generate mesh info
            for (auto&& mesh : meshes) {
                // Updata Material data
                if (auto material = mesh->GetMaterial().lock()) {
                    auto& ambient        = material->GetAmbientColor();
                    auto& diffuse        = material->GetDiffuseColor();
                    auto& emission       = material->GetEmission();
                    auto& specular       = material->GetSpecularColor();
                    auto& specular_power = material->GetSpecularPower();

                    data.ambient        = ambient.value_map ? vec4f(-1.0f) : ambient.value,
                    data.diffuse        = diffuse.value_map ? vec4f(-1.0f) : diffuse.value,
                    data.emission       = emission.value_map ? vec4f(-1.0f) : emission.value,
                    data.specular       = specular.value_map ? vec4f(-1.0f) : specular.value,
                    data.specular_power = specular_power.value_map ? -1.0f : specular_power.value,

                    m_Driver.UpdateConstantBuffer(m_ConstantBuffer, m_ConstantCount, reinterpret_cast<const uint8_t*>(&data), sizeof(data));

                    m_DrawItems.emplace_back(DrawItem{
                        pso,
                        m_ConstantCount,
                        m_ResMgr.GetMeshBuffer(*mesh),
                        ambient.value_map ? m_ResMgr.GetTextureBuffer(*ambient.value_map) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        diffuse.value_map ? m_ResMgr.GetTextureBuffer(*diffuse.value_map) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        emission.value_map ? m_ResMgr.GetTextureBuffer(*emission.value_map) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        specular.value_map ? m_ResMgr.GetTextureBuffer(*specular.value_map) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        specular_power.value_map ? m_ResMgr.GetTextureBuffer(*specular_power.value_map) : m_ResMgr.GetDefaultTextureBuffer(Format::R32_FLOAT),
                    });
                    m_ConstantCount++;
                }
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
        DebugDrawItem item{
            .pipeline = pso,
            .mesh     = std::make_shared<MeshBuffer>(),
        };

        const auto mesh = primitive.geometry->GenerateMesh();
        item.mesh->vertices.emplace("POSITION", m_Driver.CreateVertexBuffer(
                                                    "debug-primitive-position",
                                                    mesh.vertices.size(),
                                                    sizeof(decltype(mesh.vertices)::value_type),
                                                    reinterpret_cast<const uint8_t*>(mesh.vertices.data())));

        item.mesh->indices = m_Driver.CreateIndexBuffer(
            "debug-primitive-indices",
            mesh.indices.size(),
            sizeof(decltype(mesh.indices)::value_type),
            reinterpret_cast<const uint8_t*>(mesh.indices.data()));

        item.mesh->primitive = PrimitiveType::LineList;
        item.constant_offset = m_ConstantCount;

        struct {
            mat4f transform;
            vec4f color;
        } data;
        data.transform = primitive.transform;
        data.color     = primitive.color;
        m_Driver.UpdateConstantBuffer(m_ConstantBuffer, m_ConstantCount++, reinterpret_cast<const uint8_t*>(&data), sizeof(data));

        m_DebugItems.emplace_back(std::move(item));
    }
}

void Frame::SetCamera(Asset::SceneCameraNode& camera) {
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

void Frame::SetLight(Asset::SceneLightNode& light) {
    auto& data             = m_FrameConstant;
    data.light_position    = vec4f(get_origin(light.GetCalculatedTransform()), 1);
    data.light_pos_in_view = data.view * data.light_position;
    if (auto light_obj = light.GetSceneObjectRef().lock()) {
        data.light_intensity = light_obj->GetIntensity() * light_obj->GetColor().value;
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
        context->Draw(*item.buffer);
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
    m_DebugItems.clear();
    m_ConstantCount = 1;
}

}  // namespace Hitagi::Graphics