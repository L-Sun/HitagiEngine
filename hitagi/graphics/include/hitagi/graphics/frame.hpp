#pragma once
#include <hitagi/math/matrix.hpp>
#include <hitagi/resource/scene.hpp>
#include <hitagi/graphics/gpu_resource.hpp>
#include <hitagi/graphics/draw_data.hpp>

#include <unordered_map>

namespace hitagi::graphics {
class DeviceAPI;
class IGraphicsCommandContext;

class Frame {
public:
    Frame(DeviceAPI& device, std::size_t frame_index);

    void DrawScene(const resource::Scene& scene, const std::shared_ptr<resource::Texture>& render_texture = nullptr);
    void DrawDebug(const DebugDrawData& data);
    void DrawGUI(const GuiDrawData& gui_data);

    // DebugDraw
    void DrawLine(math::vec3f from, math::vec3f to, math::vec4f color);

    void BeforeSwapchainSizeChanged();
    void AfterSwapchainSizeChanged();

    void Execute();
    void Wait();
    void Reset();

private:
    ConstantBuffer& GetMaterialBuffer(const std::shared_ptr<resource::Material>& material);

    IGraphicsCommandContext* NewContext(std::string_view name);

    DeviceAPI&                                                 m_Device;
    const std::size_t                                          m_FrameIndex;
    std::pmr::vector<std::shared_ptr<IGraphicsCommandContext>> m_CommandContexts;
    std::uint64_t                                              m_FenceValue = 0;

    resource::Texture m_DepthBuffer;
    RenderTarget      m_Output;

    ConstantBuffer m_FrameCB;
    ConstantBuffer m_ObjCB;
    ConstantBuffer m_DebugCB;

    std::pmr::unordered_map<const resource::Material*, ConstantBuffer> m_MaterialBuffers;
};

}  // namespace hitagi::graphics
