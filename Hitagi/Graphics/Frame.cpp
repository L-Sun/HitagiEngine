#include "Frame.hpp"
#include "DriverAPI.hpp"
#include "ICommandContext.hpp"

namespace Hitagi::Graphics {
Frame::Frame(DriverAPI& driver, ResourceManager& resource_manager, size_t frame_index)
    : m_Driver(driver),
      m_ResMgr(resource_manager),
      m_FrameIndex(frame_index),
      m_FrameConstantBuffer(m_Driver.CreateConstantBuffer("Frame Constant", 1, sizeof(FrameConstant))),
      m_ConstantBuffer(m_Driver.CreateConstantBuffer("Object Constant", 1, sizeof(ConstantData))),
      m_MaterialBuffer(m_Driver.CreateConstantBuffer("Material Constant", 1, sizeof(MaterialData))),
      m_Output(m_Driver.CreateRenderFromSwapChain(frame_index)) {
}

void Frame::AddGeometries(std::vector<std::reference_wrapper<Asset::SceneGeometryNode>> geometries, const PipelineState& pso) {
    // Calculate need constant buffer size
    size_t constant_count = geometries.size(), material_count = 0;
    for (Asset::SceneGeometryNode& node : geometries) {
        if (auto geometry = node.GetSceneObjectRef().lock()) {
            material_count += geometry->GetMeshes().size();
        }
    }
    // if new size is smaller, the expand function return directly.
    if (m_ConstantBuffer->GetNumElements() < constant_count)
        m_ConstantBuffer = m_Driver.CreateConstantBuffer("Object Constant", constant_count, sizeof(ConstantData));
    if (m_MaterialBuffer->GetNumElements() < material_count)
        m_MaterialBuffer = m_Driver.CreateConstantBuffer("Material Constant", material_count, sizeof(MaterialData));

    for (Asset::SceneGeometryNode& node : geometries) {
        if (auto geometry = node.GetSceneObjectRef().lock()) {
            DrawItem item;

            // need update
            ConstantData data{node.GetCalculatedTransform()};
            m_Driver.UpdateConstantBuffer(m_ConstantBuffer, m_ConstantCount, reinterpret_cast<const uint8_t*>(&data), sizeof(data));
            item.constant_offset = m_ConstantCount++;

            auto& meshes = geometry->GetMeshes();
            // Generate mesh info
            for (auto&& mesh : meshes) {
                // Updata Material data
                if (auto material = mesh->GetMaterial().lock()) {
                    auto& ambient       = material->GetAmbientColor();
                    auto& diffuse       = material->GetDiffuseColor();
                    auto& emission      = material->GetEmission();
                    auto& specular      = material->GetSpecularColor();
                    auto& specular_power = material->GetSpecularPower();

                    MaterialData data{
                        ambient.value_map ? vec4f(-1.0f) : ambient.value,
                        diffuse.value_map ? vec4f(-1.0f) : diffuse.value,
                        emission.value_map ? vec4f(-1.0f) : emission.value,
                        specular.value_map ? vec4f(-1.0f) : specular.value,
                        specular_power.value_map ? -1.0f : specular_power.value,
                    };

                    m_Driver.UpdateConstantBuffer(
                        m_MaterialBuffer,
                        m_MaterialCount,
                        reinterpret_cast<const uint8_t*>(&data),
                        sizeof(data));

                    item.meshes.emplace_back(MeshInfo{
                        pso,
                        m_MaterialCount,
                        m_ResMgr.GetMeshBuffer(*mesh),
                        ambient.value_map ? m_ResMgr.GetTextureBuffer(*ambient.value_map) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        diffuse.value_map ? m_ResMgr.GetTextureBuffer(*diffuse.value_map) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        emission.value_map ? m_ResMgr.GetTextureBuffer(*emission.value_map) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        specular.value_map ? m_ResMgr.GetTextureBuffer(*specular.value_map) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        specular_power.value_map ? m_ResMgr.GetTextureBuffer(*specular_power.value_map) : m_ResMgr.GetDefaultTextureBuffer(Format::R32_FLOAT),
                    });
                    m_MaterialCount++;
                }
            }
            m_Geometries.emplace_back(std::move(item));
        }
    }
}

void Frame::AddDebugPrimitives(const std::vector<Debugger::DebugPrimitive>& primitives, const PipelineState& pso) {
    for (auto&& primitive : primitives) {
        DebugDrawItem item{
            .pipeline = pso,
            .mesh     = std::make_shared<MeshBuffer>(),
        };

        const auto [vertices, indices] = primitive.geometry->GenerateMesh();
        item.mesh->vertices.emplace("POSITION", m_Driver.CreateVertexBuffer(
                                                    "debug-primitive-position",
                                                    vertices.size(),
                                                    sizeof(decltype(vertices)::value_type),
                                                    reinterpret_cast<const uint8_t*>(vertices.data())));

        std::vector<decltype(primitive.color)> color(vertices.size(), primitive.color);
        item.mesh->vertices.emplace("COLOR", m_Driver.CreateVertexBuffer(
                                                 "debug-primitive-color",
                                                 color.size(),
                                                 sizeof(primitive.color),
                                                 reinterpret_cast<const uint8_t*>(color.data())));

        item.mesh->indices = m_Driver.CreateIndexBuffer(
            "debug-primitive-indices",
            indices.size(),
            sizeof(decltype(indices)::value_type),
            reinterpret_cast<const uint8_t*>(indices.data()));

        item.mesh->primitive = PrimitiveType::LineList;

        m_DebugItems.emplace_back(std::move(item));
    }
}

void Frame::SetCamera(Asset::SceneCameraNode& camera) {
    auto& data        = m_FrameConstant;
    data.camera_pos    = vec4f(camera.GetCameraPosition(), 1.0f);
    data.view         = camera.GetViewMatrix();
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
    data.inv_proj_view   = inverse(data.proj_view);

    m_Driver.UpdateConstantBuffer(m_FrameConstantBuffer, 0, reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

void Frame::SetLight(Asset::SceneLightNode& light) {
    auto& data          = m_FrameConstant;
    data.light_position  = vec4f(get_origin(light.GetCalculatedTransform()), 1);
    data.light_pos_in_view = data.view * data.light_position;
    if (auto light_obj = light.GetSceneObjectRef().lock()) {
        data.light_intensity = light_obj->GetIntensity() * light_obj->GetColor().value;
    }
    m_Driver.UpdateConstantBuffer(m_FrameConstantBuffer, 0, reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

void Frame::Draw(IGraphicsCommandContext* context) {
    for (const auto& item : m_Geometries) {
        for (const auto& mesh : item.meshes) {
            context->SetPipelineState(mesh.pipeline);
            context->SetParameter("FrameConstant", *m_FrameConstantBuffer, 0);
            context->SetParameter("ObjectConstants", *m_ConstantBuffer, item.constant_offset);
            context->SetParameter("MaterialConstants", *m_MaterialBuffer, mesh.material_offset);
            context->SetParameter("BaseSampler", *m_ResMgr.GetSampler("BaseSampler"));
            context->SetParameter("AmbientTexture", *mesh.ambient);
            context->SetParameter("DiffuseTexture", *mesh.diffuse);
            context->SetParameter("EmissionTexture", *mesh.emission);
            context->SetParameter("SpecularTexture", *mesh.specular);
            context->SetParameter("PowerTexture", *mesh.specular_power);
            context->Draw(*mesh.buffer);
        }
    }
}

void Frame::DebugDraw(IGraphicsCommandContext* context) {
    for (const auto& item : m_DebugItems) {
        context->SetPipelineState(item.pipeline);
        context->SetParameter("FrameConstant", *m_FrameConstantBuffer, 0);
        context->Draw(*item.mesh);
    }
}

void Frame::ResetState() {
    m_Driver.WaitFence(m_FenceValue);
    m_Geometries.clear();
    m_DebugItems.clear();
    m_ConstantCount = 0;
    m_MaterialCount = 0;
}

}  // namespace Hitagi::Graphics