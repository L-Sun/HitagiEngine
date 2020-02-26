#pragma once
#include "IRuntimeModule.hpp"

namespace My {
class InputManager : public IRuntimeModule {
public:
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
};

extern std::unique_ptr<InputManager> g_InputManager;
}  // namespace My