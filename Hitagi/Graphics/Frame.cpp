#include "Frame.hpp"
#include "DriverAPI.hpp"
#include "ICommandContext.hpp"

namespace Hitagi::Graphics {
Frame::Frame(backend::DriverAPI& driver, ResourceManager& resourceManager, size_t frameIndex)
    : m_Driver(driver),
      m_ResMgr(resourceManager),
      m_FrameIndex(frameIndex),
      m_FrameConstantBuffer(m_Driver.CreateConstantBuffer("FrameConstant", 1, sizeof(FrameConstant))),
      m_Output(m_Driver.CreateRenderFromSwapChain(frameIndex)) {
}

void Frame::SetGeometries(std::vector<std::reference_wrapper<Asset::SceneGeometryNode>> geometries) {
    m_Geometries.clear();
    m_Geometries.reserve(geometries.size());

    // Calculate need constant buffer size
    size_t constantCount = geometries.size(), materialCount = 0;
    for (Asset::SceneGeometryNode& node : geometries) {
        if (auto geometry = node.GetSceneObjectRef().lock()) {
            materialCount += geometry->GetMeshes().size();
        }
    }
    // if new size is smaller, the expand function return directly.
    if (m_ConstantBuffer.GetNumElements() < constantCount)
        m_ConstantBuffer = m_Driver.CreateConstantBuffer("Object Constant", constantCount, sizeof(ConstantData));
    if (m_MaterialBuffer.GetNumElements() < materialCount)
        m_MaterialBuffer = m_Driver.CreateConstantBuffer("Material Constant", materialCount, sizeof(MaterialData));

    size_t constantOffset = 0, materialOffset = 0;
    for (Asset::SceneGeometryNode& node : geometries) {
        if (auto geometry = node.GetSceneObjectRef().lock()) {
            DrawItem item;

            // need update
            ConstantData data{node.GetCalculatedTransform()};
            if (m_Driver.GetType() == backend::APIType::DirectX12)
                data.transform = transpose(data.transform);
            m_Driver.UpdateConstantBuffer(m_ConstantBuffer, constantOffset, reinterpret_cast<const uint8_t*>(&data), sizeof(data));
            item.constantOffset = constantOffset;
            constantOffset++;

            auto& meshes = geometry->GetMeshes();
            // Generate mesh info
            for (auto&& mesh : meshes) {
                MeshInfo info = {
                    m_ResMgr.GetMeshBuffer(*mesh),  // mesh buffer
                    materialOffset,                 // material
                    nullptr                         // TODO: pipeline
                };

                // Updata Material data
                MaterialData data = {};
                if (auto material = mesh->GetMaterial().lock()) {
                    data.ambient       = material->GetAmbientColor().Value;
                    data.diffuse       = material->GetDiffuseColor().Value;
                    data.emission      = material->GetEmission().Value;
                    data.specular      = material->GetSpecularColor().Value;
                    data.specularPower = material->GetSpecularPower().Value;

                    m_Driver.UpdateConstantBuffer(
                        m_MaterialBuffer,
                        materialOffset,
                        reinterpret_cast<const uint8_t*>(&data),
                        sizeof(data));
                }
                materialOffset++;
                item.meshes.emplace_back(info);
            }
            m_Geometries.emplace_back(std::move(item));
        }
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
    data.projection = perspectiveFov(
        cameraObject->GetFov(),
        800.0f, 600.0f,
        cameraObject->GetNearClipDistance(),
        cameraObject->GetFarClipDistance());
    data.invProjection = inverse(data.projection);
    data.projView      = data.projection * data.view;
    data.invProjView   = inverse(data.projView);

    if (m_Driver.GetType() == backend::APIType::DirectX12) {
        data.view          = transpose(data.view);
        data.projection    = transpose(data.projection);
        data.invView       = transpose(data.invView);
        data.invProjection = transpose(data.invProjection);
        data.projView      = transpose(data.projView);
        data.invProjView   = transpose(data.invProjView);
    }

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
    context->SetParameter("FrameConstant", m_FrameConstantBuffer, 0);
    for (auto&& item : m_Geometries) {
        context->SetParameter("ObjectConstants", m_ConstantBuffer, item.constantOffset);
        for (auto&& mesh : item.meshes) {
            context->SetParameter("MaterialConstants", m_MaterialBuffer, mesh.materialOffset);
            context->Draw(mesh.buffer);
        }
    }
}

}  // namespace Hitagi::Graphics