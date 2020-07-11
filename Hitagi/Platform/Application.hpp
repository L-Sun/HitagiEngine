#pragma once
#include "IRuntimeModule.hpp"
#include "Timer.hpp"

namespace Hitagi {
struct GfxConfiguration {
    GfxConfiguration(std::string_view appName = "HitagiEngine", uint32_t r = 8, uint32_t g = 8, uint32_t b = 8,
                     uint32_t a = 8, uint32_t d = 24, uint32_t s = 0, uint32_t msaa = 0, uint32_t width = 1920,
                     uint32_t height = 1080, std::string_view fontFace = "Asset/Fonts/Hasklig-Light.otf")
        : appName(appName),
          redBits(r),
          greenBits(g),
          blueBits(b),
          alphaBits(a),
          depthBits(d),
          stencilBits(s),
          msaaSample(msaa),
          screenWidth(width),
          screenHeight(height),
          fontFace(fontFace) {}

    std::string      appName;
    uint32_t         redBits;
    uint32_t         greenBits;
    uint32_t         blueBits;
    uint32_t         alphaBits;
    uint32_t         depthBits;
    uint32_t         stencilBits;
    uint32_t         msaaSample;
    uint32_t         screenWidth;
    uint32_t         screenHeight;
    std::string_view fontFace;
};

class Application : public IRuntimeModule {
public:
    struct Rect {
        long left, top, right, bottom;
    };

    Application(GfxConfiguration& cfg);
    int  Initialize() override;
    void Finalize() override;
    void Tick() override;

    const Rect& GetWindowsRect() const noexcept { return m_Rect; }

    virtual void* GetWindow() = 0;
    virtual bool  IsQuit();
    virtual void  SetCommandLineParameters(int argc, char** argv);

    virtual GfxConfiguration& GetConfiguration();
    virtual void              UpdateInputEvent() = 0;

protected:
    static bool      m_Quit;
    GfxConfiguration m_Config;
    int              m_ArgSize;
    char**           m_Arg;
    Rect             m_Rect;

    Core::Clock m_Clock;
    double      m_FPS;
    short       m_FPSLimit   = 60;
    uint64_t    m_FrameIndex = 1;

private:
    Application() = default;
};
extern std::unique_ptr<Application> g_App;
}  // namespace Hitagi
