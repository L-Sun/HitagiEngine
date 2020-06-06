#pragma once
#include "IApplication.hpp"
#include "Timer.hpp"

namespace Hitagi {
class BaseApplication : public IApplication {
public:
    BaseApplication(GfxConfiguration& cfg);
    int  Initialize() override;
    void Finalize() override;
    void Tick() override;
    bool IsQuit() override;
    void SetCommandLineParameters(int argc, char** argv) override;

    inline GfxConfiguration& GetConfiguration() final { return m_Config; }

    void UpdateInputState() override {}

protected:
    static bool      m_Quit;
    GfxConfiguration m_Config;
    int              m_ArgSize;
    char**           m_Arg;

    Core::Clock m_Clock;
    double      m_FPS;
    short       m_FPSLimit   = 60;
    uint64_t    m_FrameIndex = 1;

private:
    BaseApplication() = default;
};
}  // namespace Hitagi
