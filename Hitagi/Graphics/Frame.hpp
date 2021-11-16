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
        mat4f proj_view;
        mat4f view;
        mat4f projection;
        mat4f inv_projection;
        mat4f inv_view;
        mat4f inv_proj_view;
        vec4f camera_pos;
        vec4f light_position;
        vec4f light_pos_in_view;
        vec4f light_intensity;
    };

    struct ConstantData {
        mat4f transform;
    };

    struct MaterialData {
        vec4f ambient;
        vec4f diffuse;
        vec4f emission;
        vec4f specular;
        float specular_power;
    };

    struct MeshInfo {
        const PipelineState&           pipeline;
        size_t                         material_offset;
        std::shared_ptr<MeshBuffer>    buffer;
        std::shared_ptr<TextureBuffer> ambient;
        std::shared_ptr<TextureBuffer> diffuse;
        std::shared_ptr<TextureBuffer> emission;
        std::shared_ptr<TextureBuffer> specular;
        std::shared_ptr<TextureBuffer> specular_power;
    };

    struct DrawItem {
        std::vector<MeshInfo> meshes;
        size_t                constant_offset{};
    };

    struct DebugDrawItem {
        const PipelineState&        pipeline;
        std::shared_ptr<MeshBuffer> mesh;
    };

public:
    Frame(DriverAPI& driver, ResourceManager& resource_manager, size_t frame_index);

    void SetFenceValue(uint64_t fence_value) { m_FenceValue = fence_value; }
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

    FrameConstant                 m_FrameConstant{};
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
