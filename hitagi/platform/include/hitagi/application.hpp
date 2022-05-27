#pragma once
#include <hitagi/core/runtime_module.hpp>

#include <vector>

namespace hitagi {
class Application : public IRuntimeModule {
public:
    struct Rect {
        long left = 0, top = 0, right = 0, bottom = 0;
    };

    int  Initialize() override;
    void Finalize() override;
    void Tick() override;

    const Rect& GetWindowsRect() const noexcept { return m_Rect; }
    bool        WindowSizeChanged() const noexcept { return m_SizeChanged; }

    virtual void* GetWindow()                                    = 0;
    virtual void  InitializeWindows()                            = 0;
    virtual void  SetInputScreenPosition(unsigned x, unsigned y) = 0;

    virtual float GetDpiRatio() = 0;
    virtual bool  IsQuit();
    virtual void  SetCommandLineParameters(int argc, char** argv);

protected:
    static bool sm_Quit;
    bool        m_Initialized = false;
    int         m_ArgSize     = 0;
    char**      m_Arg         = nullptr;
    Rect        m_Rect{};
    bool        m_SizeChanged = false;

    std::vector<IRuntimeModule*> m_Modules;
};
extern std::unique_ptr<Application> g_App;
}  // namespace hitagi
