#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/graphics/device_api.hpp>
#include <hitagi/graphics/enums.hpp>
#include <hitagi/graphics/resource_manager.hpp>
#include <hitagi/graphics/frame.hpp>
#include <hitagi/resource/renderable.hpp>
#include <hitagi/resource/camera.hpp>

namespace hitagi::graphics {
class Frame;

class GraphicsManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void SetCamera(std::shared_ptr<resource::Camera> camera);
    void AppendRenderables(std::pmr::vector<resource::Renderable> renderables);

protected:
    // TODO change the parameter to View, if multiple view port is finished
    void   OnSizeChanged();
    Frame* GetBcakFrameForRendering();

    void Render();

    std::unique_ptr<DeviceAPI>       m_Device;
    std::unique_ptr<ResourceManager> m_ResMgr;

    static constexpr uint8_t sm_SwapChianSize    = 3;
    static constexpr Format  sm_BackBufferFormat = Format::R8G8B8A8_UNORM;
    int                      m_CurrBackBuffer    = 0;

    // TODO multiple RenderTarget is need if the application has multiple view port
    // if the class View is impletement, RenderTarget will be a member variable of View
    std::array<std::unique_ptr<Frame>, sm_SwapChianSize> m_Frames;

    std::shared_ptr<resource::Camera> m_Camera;
};

}  // namespace hitagi::graphics
namespace hitagi {
extern std::unique_ptr<graphics::GraphicsManager> g_GraphicsManager;
}