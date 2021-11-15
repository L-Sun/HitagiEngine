#include "Frame.hpp"
#include "DriverAPI.hpp"
#include "ICommandContext.hpp"

namespace Hitagi::Graphics {
Frame::Frame(DriverAPI& driver, ResourceManager& resourceManager, size_t frameIndex)
    : m_Driver(driver),
      m_ResMgr(resourceManager),
      m_FrameIndex(frameIndex),
      m_FrameConstantBuffer(m_Driver.CreateConstantBuffer("Frame Constant", 1, sizeof(FrameConstant))),
      m_ConstantBuffer(m_Driver.CreateConstantBuffer("Object Constant", 1, sizeof(ConstantData))),
      m_MaterialBuffer(m_Driver.CreateConstantBuffer("Material Constant", 1, sizeof(MaterialData))),
      m_Output(m_Driver.CreateRenderFromSwapChain(frameIndex)) {
}

void Frame::AddGeometries(std::vector<std::reference_wrapper<Asset::SceneGeometryNode>> geometries, const PipelineState& pso) {
    // Calculate need constant buffer size
    size_t constantCount = geometries.size(), materialCount = 0;
    for (Asset::SceneGeometryNode& node : geometries) {
        if (auto geometry = node.GetSceneObjectRef().lock()) {
            materialCount += geometry->GetMeshes().size();
        }
    }
    // if new size is smaller, the expand function return directly.
    if (m_ConstantBuffer->GetNumElements() < constantCount)
        m_ConstantBuffer = m_Driver.CreateConstantBuffer("Object Constant", constantCount, sizeof(ConstantData));
    if (m_MaterialBuffer->GetNumElements() < materialCount)
        m_MaterialBuffer = m_Driver.CreateConstantBuffer("Material Constant", materialCount, sizeof(MaterialData));

    for (Asset::SceneGeometryNode& node : geometries) {
        if (auto geometry = node.GetSceneObjectRef().lock()) {
            DrawItem item;

            // need update
            ConstantData data{node.GetCalculatedTransform()};
            m_Driver.UpdateConstantBuffer(m_ConstantBuffer, m_ConstantCount, reinterpret_cast<const uint8_t*>(&data), sizeof(data));
            item.constantOffset = m_ConstantCount++;

            auto& meshes = geometry->GetMeshes();
            // Generate mesh info
            for (auto&& mesh : meshes) {
                // Updata Material data
                if (auto material = mesh->GetMaterial().lock()) {
                    auto& ambient       = material->GetAmbientColor();
                    auto& diffuse       = material->GetDiffuseColor();
                    auto& emission      = material->GetEmission();
                    auto& specular      = material->GetSpecularColor();
                    auto& specularPower = material->GetSpecularPower();

                    MaterialData data{
                        ambient.ValueMap ? vec4f(-1.0f) : ambient.Value,
                        diffuse.ValueMap ? vec4f(-1.0f) : diffuse.Value,
                        emission.ValueMap ? vec4f(-1.0f) : emission.Value,
                        specular.ValueMap ? vec4f(-1.0f) : specular.Value,
                        specularPower.ValueMap ? -1.0f : specularPower.Value,
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
                        ambient.ValueMap ? m_ResMgr.GetTextureBuffer(*ambient.ValueMap) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        diffuse.ValueMap ? m_ResMgr.GetTextureBuffer(*diffuse.ValueMap) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        emission.ValueMap ? m_ResMgr.GetTextureBuffer(*emission.ValueMap) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        specular.ValueMap ? m_ResMgr.GetTextureBuffer(*specular.ValueMap) : m_ResMgr.GetDefaultTextureBuffer(Format::R8G8_B8G8_UNORM),
                        specularPower.ValueMap ? m_ResMgr.GetTextureBuffer(*specularPower.ValueMap) : m_ResMgr.GetDefaultTextureBuffer(Format::R32_FLOAT),
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
    data.cameraPos    = vec4f(camera.GetCameraPosition(), 1.0f);
    data.view         = camera.GetViewMatrix();
    data.invView      = inverse(data.view);
    auto cameraObject = camera.GetSceneObjectRef().lock();
    assert(cameraObject != nullptr);
    // TODO orth camera
    data.projection = perspective(
        cameraObject->GetFov(),
        cameraObject->GetAspect(),
        cameraObject->GetNearClipDistance(),
        cameraObject->GetFarClipDistance());
    data.invProjection = inverse(data.projection);
    data.projView      = data.projection * data.view;
    data.invProjView   = inverse(data.projView);

    m_Driver.UpdateConstantBuffer(m_FrameConstantBuffer, 0, reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

void Frame::SetLight(Asset::SceneLightNode& light) {
    auto& data          = m_FrameConstant;
    data.lightPosition  = vec4f(GetOrigin(light.GetCalculatedTransform()), 1);
    data.lightPosInView = data.view * data.lightPosition;
    if (auto lightObj = light.GetSceneObjectRef().lock()) {
        data.lightIntensity = lightObj->GetIntensity() * lightObj->GetColor().Value;
    }
    m_Driver.UpdateConstantBuffer(m_FrameConstantBuffer, 0, reinterpret_cast<uint8_t*>(&data), sizeof(data));
}

void Frame::Draw(IGraphicsCommandContext* context) {
    for (const auto& item : m_Geometries) {
        for (const auto& mesh : item.meshes) {
            context->SetPipelineState(mesh.pipeline);
            context->SetParameter("FrameConstant", *m_FrameConstantBuffer, 0);
            context->SetParameter("ObjectConstants", *m_ConstantBuffer, item.constantOffset);
            context->SetParameter("MaterialConstants", *m_MaterialBuffer, mesh.materialOffset);
            context->SetParameter("BaseSampler", *m_ResMgr.GetSampler("BaseSampler"));
            context->SetParameter("AmbientTexture", *mesh.ambient);
            context->SetParameter("DiffuseTexture", *mesh.diffuse);
            context->SetParameter("EmissionTexture", *mesh.emission);
            context->SetParameter("SpecularTexture", *mesh.specular);
            context->SetParameter("PowerTexture", *mesh.specularPower);
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