#pragma once
#include "SceneNode.hpp"
#include "ResourceManager.hpp"
#include "PipelineState.hpp"

#include "DebugManager.hpp"
#include "GuiManager.hpp"
#include <Math/Vector.hpp>
#include <Math/Matrix.hpp>

#include <vector>

namespace Hitagi::Graphics {
class DriverAPI;
class IGraphicsCommandContext;

class Frame {
    struct FrameConstant {
        Math::mat4f proj_view;
        Math::mat4f view;
        Math::mat4f projection;
        Math::mat4f inv_projection;
        Math::mat4f inv_view;
        Math::mat4f inv_proj_view;
        Math::vec4f camera_pos;
        Math::vec4f light_position;
        Math::vec4f light_pos_in_view;
        Math::vec4f light_intensity;
    };
    struct ObjectConstant {
        Math::mat4f transform;
        Math::vec4f ambient;
        Math::vec4f diffuse;
        Math::vec4f emission;
        Math::vec4f specular;
        float       specular_power;
    };

    struct DrawItem {
        const PipelineState&           pipeline;
        size_t                         constant_offset;
        std::shared_ptr<MeshBuffer>    mesh;
        std::shared_ptr<TextureBuffer> ambient;
        std::shared_ptr<TextureBuffer> diffuse;
        std::shared_ptr<TextureBuffer> emission;
        std::shared_ptr<TextureBuffer> specular;
        std::shared_ptr<TextureBuffer> specular_power;
    };

    struct GuiDrawInformation {
        const PipelineState&                     pipeline;
        std::vector<std::shared_ptr<MeshBuffer>> meshes;
        std::vector<std::array<uint32_t, 4>>     scissor_rects;
        std::shared_ptr<TextureBuffer>           font_texture;
        size_t                                   constant_offset;
    };

    struct DebugDrawItem {
        const PipelineState&        pipeline;
        std::shared_ptr<MeshBuffer> mesh;
        size_t                      constant_offset;
    };

public:
    Frame(DriverAPI& driver, ResourceManager& resource_manager, size_t frame_index);

    void SetFenceValue(uint64_t fence_value) { m_FenceValue = fence_value; }
    // TODO generate pipeline state object from scene node infomation
    void AddGeometries(const std::vector<std::shared_ptr<Asset::GeometryNode>>& geometries, const PipelineState& pso);
    void AddDebugPrimitives(const std::vector<Debugger::DebugPrimitive>& primitives, const PipelineState& pso);
    void PrepareImGuiData(ImDrawData* data, std::shared_ptr<Asset::Image> font_texture, const PipelineState& pso);
    void SetCamera(Asset::CameraNode& camera);
    void SetLight(Asset::LightNode& light);
    void Draw(IGraphicsCommandContext* context);
    void GuiDraw(IGraphicsCommandContext* context);
    void DebugDraw(IGraphicsCommandContext* context);

    void ResetState();

    void        SetRenderTarget(std::shared_ptr<RenderTarget> rt);
    inline auto GetRenderTarget() noexcept { return m_Output; }

private:
    void PopulateMaterial(const Asset::Material::Color& color, Math::vec4f& value_dest, std::shared_ptr<TextureBuffer>& texture_dest);
    void PopulateMaterial(const Asset::Material::SingleValue& color, float& value_dest, std::shared_ptr<TextureBuffer>& texture_dest);

    DriverAPI&       m_Driver;
    ResourceManager& m_ResMgr;
    const size_t     m_FrameIndex;
    uint64_t         m_FenceValue = 0;

    FrameConstant                       m_FrameConstant{};
    std::vector<DrawItem>               m_DrawItems;
    std::vector<DebugDrawItem>          m_DebugItems;
    std::shared_ptr<GuiDrawInformation> m_GuiDrawInfo;
    std::shared_ptr<RenderTarget>       m_Output;

    // the constant data used among the frame, including camera, light, etc.
    // TODO object constant buffer layout depending on different object
    std::shared_ptr<ConstantBuffer> m_ConstantBuffer;
    // the first element in constant buffer is frame constant, including camera light
    size_t m_ConstantCount = 1;
};

}  // namespace Hitagi::Graphics
