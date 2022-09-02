#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/graphics/device_api.hpp>
#include <hitagi/graphics/enums.hpp>
#include <hitagi/graphics/frame.hpp>
#include <hitagi/resource/scene.hpp>
#include <hitagi/graphics/draw_data.hpp>

namespace hitagi::graphics {
class GraphicsManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;

    // If texture is set, the scene will be render to texture
    void DrawScene(const resource::Scene& scene, const std::shared_ptr<resource::Texture>& render_texture = nullptr);
    void DrawDebug(const DebugDrawData& debug_data);
    void DrawGui(const GuiDrawData& gui_data);

    PipelineState& GetPipelineState(const resource::Material* material);

    struct {
        PipelineState gui;
        PipelineState debug;
    } builtin_pipeline;

protected:
    // TODO change the parameter to View, if multiple view port is finished
    void OnSizeChanged();
    void InitBuiltInPipeline();

    std::unique_ptr<DeviceAPI> m_Device;

    static constexpr std::uint8_t     sm_SwapChianSize    = 3;
    static constexpr resource::Format sm_BackBufferFormat = resource::Format::R8G8B8A8_UNORM;
    int                               m_CurrBackBuffer    = 0;

    // TODO multiple RenderTarget is need if the application has multiple view port
    // if the class View is impletement, RenderTarget will be a member variable of View
    std::array<std::unique_ptr<Frame>, sm_SwapChianSize> m_Frames;

    std::pmr::unordered_map<const resource::Material*, PipelineState> m_Pipelines;
};

}  // namespace hitagi::graphics
namespace hitagi {
extern graphics::GraphicsManager* graphics_manager;
}