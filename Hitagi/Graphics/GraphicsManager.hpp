#pragma once
#include "DriverAPI.hpp"
#include "ResourceManager.hpp"
#include "Types.hpp"
#include "Frame.hpp"
#include "PipelineState.hpp"

#include "IRuntimeModule.hpp"
#include "Scene.hpp"
#include "ShaderManager.hpp"

namespace hitagi::graphics {
class GraphicsManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

protected:
    // TODO change the parameter to View, if multiple view port is finished
    void   Render(const asset::Scene& scene);
    void   OnSizeChanged();
    Frame* GetBcakFrameForRendering();

    std::unique_ptr<DriverAPI>       m_Driver;
    std::unique_ptr<ResourceManager> m_ResMgr;

    static constexpr uint8_t sm_SwapChianSize    = 3;
    static constexpr Format  sm_BackBufferFormat = Format::R8G8B8A8_UNORM;
    int                      m_CurrBackBuffer    = 0;

    // TODO use file to descript pipeline object
    std::unique_ptr<PipelineState> m_PSO;
    std::unique_ptr<PipelineState> m_ImGuiPSO;
    std::unique_ptr<PipelineState> m_DebugPSO;
    std::unique_ptr<PipelineState> m_DebugDepthDisabledPSO;
    ShaderManager                  m_ShaderManager;

    // TODO multiple RenderTarget is need if the application has multiple view port
    // if the class View is impletement, RenderTarget will be a member variable of View
    std::array<std::unique_ptr<Frame>, sm_SwapChianSize> m_Frames;
};

}  // namespace hitagi::graphics
namespace hitagi {
extern std::unique_ptr<graphics::GraphicsManager> g_GraphicsManager;
}