#pragma once
#include "IApplication.hpp"
#include "IPhysicsManager.hpp"
#include "GraphicsManager.hpp"
#include "MemoryManager.hpp"
#include "AssetLoader.hpp"
#include "SceneManager.hpp"
#include "InputManager.hpp"
#include "DebugManager.hpp"
#include "GameLogic.hpp"
#include "Timer.hpp"

namespace My {
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
    static bool      m_bQuit;
    GfxConfiguration m_Config;
    int              m_nArgC;
    char**           m_ppArgV;

    Clock m_clock;
    short m_FPS = 60;

private:
    BaseApplication() {}
    short m_sumFPS        = 0;
    short m_frame_counter = -1;
    long  m_k             = 0;
};
}  // namespace My
