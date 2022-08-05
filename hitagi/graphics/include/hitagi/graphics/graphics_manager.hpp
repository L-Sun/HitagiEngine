#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/graphics/device_api.hpp>
#include <hitagi/graphics/enums.hpp>
#include <hitagi/graphics/frame.hpp>
#include <hitagi/resource/scene.hpp>

namespace hitagi::graphics {
class GraphicsManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;

    void Draw(const resource::Scene& scene);
    void AppendRenderables(std::pmr::vector<resource::Renderable> renderables);

    PipelineState& GetPipelineState(const resource::Material* material);

protected:
    // TODO change the parameter to View, if multiple view port is finished
    void   OnSizeChanged();
    Frame* GetBcakFrameForRendering();

    void Render();

    std::unique_ptr<DeviceAPI> m_Device;

    static constexpr uint8_t          sm_SwapChianSize    = 2;
    static constexpr resource::Format sm_BackBufferFormat = resource::Format::R8G8B8A8_UNORM;
    int                               m_CurrBackBuffer    = 0;

    const resource::Scene* m_CurrScene = nullptr;

    // TODO multiple RenderTarget is need if the application has multiple view port
    // if the class View is impletement, RenderTarget will be a member variable of View
    std::array<std::unique_ptr<Frame>, sm_SwapChianSize> m_Frames;

    std::pmr::unordered_map<const resource::Material*, PipelineState> m_Pipelines;
};

}  // namespace hitagi::graphics
namespace hitagi {
extern graphics::GraphicsManager* graphics_manager;
}