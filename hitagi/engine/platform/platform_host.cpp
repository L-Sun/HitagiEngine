export module platform:host;
import interop.spdlog;

import std;
import utils;
import math;
import core;
import hid;

export namespace hitagi::platform {
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

enum class Surface { Vulkan,
                     Metal };
struct PlatformConfig {
    Surface surface =
#ifdef __APPLE__
        Surface::Metal;
#else
        Surface::Vulkan;
#endif
    std::pmr::string title     = "Hitagi Engine";
    std::pmr::string version   = "v0.2.0";
    std::uint32_t    width     = 800;
    std::uint32_t    height    = 800;
    bool             maximized = false;
    bool             headless  = false;
};

class PlatformHost {
public:
    struct Rect {
        std::uint32_t left = 0, top = 0, right = 0, bottom = 0;
    };
    PlatformHost(PlatformConfig config);
    virtual ~PlatformHost() = default;

    static auto Create(PlatformConfig config = {}) -> std::unique_ptr<PlatformHost>;

    virtual void PollEvents();
    // A native modal move/resize loop may request a frame on the same thread.
    // The callback must advance simulation/rendering without polling native events.
    void SetModalFrameCallback(std::function<void()> callback) { m_ModalFrame = std::move(callback); }

    virtual void SetInputScreenPosition(const math::vec2u& position) = 0;
    // Zero ends the focused text-edit session. Platform implementations cancel
    // any outstanding composition before changing its owner.
    virtual void SetTextInputSession(std::uint64_t session) { m_Input->SetTextInputSession(session); }
    virtual void SetWindowTitle(std::string_view name)                   = 0;
    virtual void SetCursor(Cursor cursor)                                = 0;
    virtual void SetMousePosition(const math::vec2u& position)           = 0;
    virtual void ResizeWindow(std::uint32_t width, std::uint32_t height) = 0;

    virtual auto GetWindow() const -> utils::Window    = 0;
    virtual auto GetDpiRatio() const -> float          = 0;
    virtual auto GetMemoryUsage() const -> std::size_t = 0;
    virtual auto GetWindowRect() const -> Rect         = 0;
    virtual bool WindowSizeChanged() const             = 0;
    virtual bool WindowsMinimized() const              = 0;
    virtual bool WindowMaximized() const               = 0;
    virtual bool IsQuit() const                        = 0;
    virtual void Quit()                                = 0;

    auto GetWindowWidth() const -> std::uint32_t;
    auto GetWindowHeight() const -> std::uint32_t;

    inline auto GetConfig() const noexcept { return m_Config; }

    inline auto& GetInputState() const noexcept { return *m_Input; }

protected:
    bool                             TickModalFrame() noexcept;
    core::Clock                      m_Clock;
    PlatformConfig                   m_Config;
    std::unique_ptr<hid::InputState> m_Input;
    std::shared_ptr<spdlog::logger>  m_Logger;

private:
    std::function<void()> m_ModalFrame;
    std::exception_ptr    m_ModalFrameError;
    bool                  m_InModalFrame = false;
};
}  // namespace hitagi::platform

namespace hitagi::platform {
PlatformHost::PlatformHost(PlatformConfig config)
    : m_Config(std::move(config)),
      m_Input(std::make_unique<hid::InputState>()),
      m_Logger(utils::try_create_logger(m_Config.title)) {
    m_Clock.Start();
}

void PlatformHost::PollEvents() {
    if (m_ModalFrameError) std::rethrow_exception(std::exchange(m_ModalFrameError, {}));
    m_Clock.Tick();
    m_Config.maximized = WindowMaximized();
    if (WindowSizeChanged() && !WindowsMinimized() && !m_Config.maximized) {
        m_Config.width  = GetWindowWidth();
        m_Config.height = GetWindowHeight();
    }
    m_Input->AdvanceFrame();
}

bool PlatformHost::TickModalFrame() noexcept {
    if (!m_ModalFrame || m_InModalFrame || m_ModalFrameError || IsQuit()) return false;
    m_InModalFrame = true;
    try {
        PlatformHost::PollEvents();
        m_ModalFrame();
    } catch (...) {
        // Never unwind a C++ exception through a native window procedure or SDL callback.
        m_ModalFrameError = std::current_exception();
    }
    m_InModalFrame = false;
    return true;
}

auto PlatformHost::GetWindowWidth() const -> std::uint32_t {
    const auto rect = GetWindowRect();
    return rect.right - rect.left;
}

auto PlatformHost::GetWindowHeight() const -> std::uint32_t {
    const auto rect = GetWindowRect();
    return rect.bottom - rect.top;
}

}  // namespace hitagi::platform
