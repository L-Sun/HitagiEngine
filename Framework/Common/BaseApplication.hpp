#pragma once
#include "IApplication.hpp"
#include "GraphicsManager.hpp"
#include "MemoryManager.hpp"
#include "AssetLoader.hpp"
#include "SceneManager.hpp"
#include "InputManager.hpp"
#include "PhysicsManager.hpp"

namespace My {
class BaseApplication : implements IApplication {
public:
    BaseApplication(GfxConfiguration& cfg);
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();
    virtual bool IsQuit();

    virtual void SetCommandLineParameters(int argc, char** argv);
    virtual int  LoadScene();
    virtual void OnDraw() {}

    inline GfxConfiguration& GetConfiguration() { return m_Config; }

protected:
    static bool      m_bQuit;
    GfxConfiguration m_Config;
    int              m_nArgC;
    char**           m_ppArgV;

private:
    BaseApplication() {}
};
}  // namespace My
