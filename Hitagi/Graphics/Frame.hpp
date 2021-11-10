#pragma once
#include "SceneNode.hpp"
#include "ResourceManager.hpp"
#include "PipelineState.hpp"
#include "DebugManager.hpp"

#include "DebugManager.hpp"

#include <vector>

namespace Hitagi::Graphics {
class DriverAPI;
class IGraphicsCommandContext;

class Frame {
    // TODO multiple light
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

public:
    Frame(DriverAPI& driver, ResourceManager& resourceManager, size_t frameIndex);

    void SetFenceValue(uint64_t fenceValue) { m_FenceValue = fenceValue; }
    void SetGeometries(std::vector<std::reference_wrapper<Asset::SceneGeometryNode>> geometries);
    void SetDebugPrimitives(const std::vector<Debugger::DebugPrimitive>& primitives);
    void SetCamera(Asset::SceneCameraNode& camera);
    void SetLight(Asset::SceneLightNode& light);
    void Draw(IGraphicsCommandContext* context);

    void ResetState();

    RenderTarget& GetRenderTarget() { return m_Output; }

private:
    DriverAPI&       m_Driver;
    ResourceManager& m_ResMgr;
    size_t           m_FrameIndex;
    uint64_t         m_FenceValue = 0;

    FrameConstant         m_FrameConstant;
    std::vector<DrawItem> m_Geometries;
    RenderTarget          m_Output;

    // the constant data used among the frame, including camera, light, etc.
    ConstantBuffer m_FrameConstantBuffer;
    ConstantBuffer m_ConstantBuffer;
    ConstantBuffer m_MaterialBuffer;
};
}  // namespace Hitagi::Graphics
