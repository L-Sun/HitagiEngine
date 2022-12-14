#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/math/vector.hpp>

#include <vector>

namespace hitagi {
enum struct Cursor : std::uint8_t {
    None,
    Arrow,
    TextInput,
    ResizeAll,
    ResizeEW,
    ResizeNS,
    ResizeNESW,
    ResizeNWSE,
    Hand,
    Forbid,
};

class Application : public RuntimeModule {
public:
    struct Rect {
        std::uint32_t left = 0, top = 0, right = 0, bottom = 0;
    };

    bool Initialize() override;
    void Tick() override;

    inline std::string_view GetName() const noexcept override { return "Application"; }

    static std::unique_ptr<Application> CreateApp();

    virtual void InitializeWindows()                                 = 0;
    virtual void SetInputScreenPosition(const math::vec2u& position) = 0;
    virtual void SetWindowTitle(std::string_view name)               = 0;
    virtual void SetCursor(Cursor cursor)                            = 0;
    virtual void SetMousePosition(const math::vec2u& position)       = 0;

    virtual void*       GetWindow()               = 0;
    virtual float       GetDpiRatio() const       = 0;
    virtual std::size_t GetMemoryUsage() const    = 0;
    virtual Rect        GetWindowsRect() const    = 0;
    virtual bool        WindowSizeChanged() const = 0;
    virtual bool        IsQuit() const            = 0;
    virtual void        Quit()                    = 0;

protected:
    core::Clock m_Clock;
};
extern Application* app;
}  // namespace hitagi
