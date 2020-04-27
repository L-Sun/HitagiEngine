#pragma once
#include "IApplication.hpp"
#include "Timer.hpp"

namespace Hitagi {
class BaseApplication : public IApplication {
public:
    BaseApplication(GfxConfiguration& cfg);
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();
    virtual bool IsQuit();

    virtual void SetCommandLineParameters(int argc, char** argv);
    virtual void OnDraw() {}

    inline GfxConfiguration& GetConfiguration() { return m_Config; }

protected:
    static bool      m_Quit;
    GfxConfiguration m_Config;
    int              m_ArgSize;
    char**           m_Arg;

    Core::Clock m_Clock;
    double      m_FPS;
    short       m_FPSLimit = 120;

private:
    BaseApplication() {}
};
}  // namespace Hitagi
