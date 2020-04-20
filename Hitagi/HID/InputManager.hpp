#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "IRuntimeModule.hpp"

namespace Hitagi {
class InputManager : public IRuntimeModule {
public:
    InputManager() : m_Logger(spdlog::stdout_color_st("InputManager")) {
#if defined(_DEBUG)
        m_Logger->set_level(spdlog::level::debug);
#endif  // _DEBUG
    }
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    void UpArrowKeyDown();
    void UpArrowKeyUp();
    void DownArrowKeyDown();
    void DownArrowKeyUp();
    void LeftArrowKeyDown();
    void LeftArrowKeyUp();
    void RightArrowKeyDown();
    void RightArrowKeyUp();
    void CKeyUp();
    void CKeyDown();

    void ResetKeyUp();
    void ResetKeyDown();
    void DebugKeyUp();
    void DebugKeyDown();

protected:
    bool m_UpKeyPressed    = false;
    bool m_DownKeyPressed  = false;
    bool m_LeftKeyPressed  = false;
    bool m_RightKeyPressed = false;
    bool m_CKeyPressed     = false;

    std::shared_ptr<spdlog::logger> m_Logger;
};

extern std::unique_ptr<InputManager> g_InputManager;
}  // namespace Hitagi