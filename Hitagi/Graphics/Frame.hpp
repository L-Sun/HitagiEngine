#pragma once
#include "SceneNode.hpp"
#include "ResourceManager.hpp"
#include "PipelineState.hpp"

#include <vector>

namespace Hitagi::Graphics {
namespace backend {
class DriverAPI;
}
class IGraphicsCommandContext;

class Frame {
public:
    Frame(backend::DriverAPI& driver, ResourceManager& resourceManager, size_t frameIndex);
    void          SetGeometries(std::vector<std::reference_wrapper<Asset::SceneGeometryNode>> geometries);
    void          SetCamera(Asset::SceneCameraNode& camera);
    void          SetLight(Asset::SceneLightNode& light);
    RenderTarget& GetRenerTarget() { return m_Output; }
    void          Draw(IGraphicsCommandContext* context);

    struct FrameConstant {
        // Camera
        mat4f projView;
        mat4f view;
        mat4f projection;
        mat4f invProjection;
        mat4f invView;
        mat4f invProjView;
        vec4f cameraPos;
        vec4f lightPosition;
        vec4f lightPosInView;
        vec4f lightIntensity;
    };

    struct ConstantData {
        mat4f transform;
    };

    struct MaterialData {
        vec4f ambient;
        vec4f diffuse;
        vec4f emission;
        vec4f specular;
        float specularPower;
    };

    struct MeshInfo {
        const MeshBuffer&    buffer;
        size_t               materialOffset;
        const TextureBuffer& ambient;
        const TextureBuffer& diffuse;
        const TextureBuffer& emission;
        const TextureBuffer& specular;
        const TextureBuffer& specularPower;
        const PipelineState* pipeline;
    };

    struct DrawItem {
        std::vector<MeshInfo> meshes;
        size_t                constantOffset;
    };

private:
    backend::DriverAPI& m_Driver;
    ResourceManager&    m_ResMgr;
    size_t              m_FrameIndex;

    FrameConstant         m_FrameConstant;
    std::vector<DrawItem> m_Geometries;
    RenderTarget          m_Output;

    // the constant data used among the frame, including camera, light, etc.
    ConstantBuffer m_FrameConstantBuffer;
    ConstantBuffer m_ConstantBuffer;
    ConstantBuffer m_MaterialBuffer;
};
}  // namespace Hitagi::Graphics
