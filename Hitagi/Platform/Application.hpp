#pragma once
#include "IRuntimeModule.hpp"
#include "Timer.hpp"

namespace hitagi {
struct GfxConfiguration {
    GfxConfiguration(std::string_view app_name = "hitagiEngine", uint32_t r = 8, uint32_t g = 8, uint32_t b = 8,
                     uint32_t a = 8, uint32_t d = 24, uint32_t s = 0, uint32_t msaa = 0, uint32_t width = 1920,
                     uint32_t height = 1080, std::string_view font_face = "Assets/Fonts/Hasklig-Light.otf")
        : app_name(app_name),
          red_bits(r),
          green_bits(g),
          blue_bits(b),
          alpha_bits(a),
          depth_bits(d),
          stencil_bits(s),
          msaa_sample(msaa),
          screen_width(width),
          screen_height(height),
          font_face(font_face) {}

    std::string      app_name;
    uint32_t         red_bits;
    uint32_t         green_bits;
    uint32_t         blue_bits;
    uint32_t         alpha_bits;
    uint32_t         depth_bits;
    uint32_t         stencil_bits;
    uint32_t         msaa_sample;
    uint32_t         screen_width;
    uint32_t         screen_height;
    std::string_view font_face;
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
    bool        WindowSizeChanged() const noexcept { return m_SizeChanged; }

    virtual void* GetWindow()                                    = 0;
    virtual void  SetInputScreenPosition(unsigned x, unsigned y) = 0;
    // Get Dpi / 96.0f
    virtual float GetDpiRatio() = 0;
    virtual bool  IsQuit();
    virtual void  SetCommandLineParameters(int argc, char** argv);

    virtual GfxConfiguration& GetConfiguration();

protected:
    void OnResize();

    static bool      sm_Quit;
    bool             m_Initialized = false;
    GfxConfiguration m_Config;
    int              m_ArgSize = 0;
    char**           m_Arg     = nullptr;
    Rect             m_Rect{};
    bool             m_SizeChanged = false;
};
extern std::unique_ptr<Application> g_App;
}  // namespace hitagi
