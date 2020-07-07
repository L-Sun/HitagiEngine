#pragma once
#include "DriverAPI.hpp"
#include "ResourceManager.hpp"
#include "Format.hpp"
#include "Frame.hpp"
#include "PipelineState.hpp"

#include "IRuntimeModule.hpp"
#include "Scene.hpp"
#include "ShaderManager.hpp"

namespace Hitagi::Graphics {
class GraphicsManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

protected:
    void Render(const Asset::Scene& secene);

    std::unique_ptr<backend::DriverAPI> m_Driver;
    std::unique_ptr<ResourceManager>    m_ResMgr;

    static constexpr uint8_t sm_FrameCount       = 3;
    static constexpr Format  sm_BackBufferFormat = Format::R8G8B8A8_UNORM;
    int                      m_CurrBackBuffer    = 0;

    std::array<std::unique_ptr<Frame>, sm_FrameCount> m_Frame;
    std::unique_ptr<PipelineState>                    m_PSO;
    ShaderManager                                     m_ShaderManager;
};

}  // namespace Hitagi::Graphics
namespace Hitagi {
extern std::unique_ptr<Graphics::GraphicsManager> g_GraphicsManager;
}