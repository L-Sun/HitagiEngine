#pragma once
#include "SceneNode.hpp"
#include "ResourceManager.hpp"
#include "PipelineState.hpp"

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
        const PipelineState&           pipeline;
        size_t                         materialOffset;
        std::shared_ptr<MeshBuffer>    buffer;
        std::shared_ptr<TextureBuffer> ambient;
        std::shared_ptr<TextureBuffer> diffuse;
        std::shared_ptr<TextureBuffer> emission;
        std::shared_ptr<TextureBuffer> specular;
        std::shared_ptr<TextureBuffer> specularPower;
    };

    struct DrawItem {
        std::vector<MeshInfo> meshes;
        size_t                constantOffset;
    };

    struct DebugDrawItem {
        const PipelineState&        pipeline;
        std::shared_ptr<MeshBuffer> mesh;
    };

public:
    Frame(DriverAPI& driver, ResourceManager& resourceManager, size_t frameIndex);

    void SetFenceValue(uint64_t fenceValue) { m_FenceValue = fenceValue; }
    // TODO generate pipeline state object from scene node infomation
    void AddGeometries(std::vector<std::reference_wrapper<Asset::SceneGeometryNode>> geometries, const PipelineState& pso);
    void AddDebugPrimitives(const std::vector<Debugger::DebugPrimitive>& primitives, const PipelineState& pso);
    void SetCamera(Asset::SceneCameraNode& camera);
    void SetLight(Asset::SceneLightNode& light);
    void Draw(IGraphicsCommandContext* context);
    void DebugDraw(IGraphicsCommandContext* context);

    void ResetState();

    auto GetRenderTarget() { return m_Output; }

private:
    DriverAPI&       m_Driver;
    ResourceManager& m_ResMgr;
    size_t           m_FrameIndex;
    uint64_t         m_FenceValue = 0;

    FrameConstant                 m_FrameConstant;
    std::vector<DrawItem>         m_Geometries;
    std::vector<DebugDrawItem>    m_DebugItems;
    std::shared_ptr<RenderTarget> m_Output;

    // the constant data used among the frame, including camera, light, etc.
    std::shared_ptr<ConstantBuffer> m_FrameConstantBuffer;
    std::shared_ptr<ConstantBuffer> m_ConstantBuffer;
    std::shared_ptr<ConstantBuffer> m_MaterialBuffer;
    size_t                          m_ConstantCount = 0;
    size_t                          m_MaterialCount = 0;
};
}  // namespace Hitagi::Graphics
