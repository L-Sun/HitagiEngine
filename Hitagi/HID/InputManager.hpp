#pragma once
#include "IRuntimeModule.hpp"
#include "InputEvent.hpp"

#include <variant>
#include <unordered_map>

namespace Hitagi {

class InputManager : public IRuntimeModule {
public:
    using UserDefAction = int;
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void UpdateState(VirtualKeyCode key, bool state) noexcept { m_KeyState[static_cast<size_t>(key)].current = state; }
    void UpdateState(std::array<float, 2> position) noexcept { m_MouseState.currPos = position; }

    void Map(UserDefAction userAction, std::variant<VirtualKeyCode, MouseEvent> event);

    bool  GetBool(UserDefAction userAction) const;
    bool  GetBoolNew(UserDefAction userAction) const;
    float GetFloat(UserDefAction userAction) const;
    float GetFloatDelta(UserDefAction userAction) const;

private:
    std::array<KeyState, static_cast<size_t>(VirtualKeyCode::NUM)> m_KeyState;
    MouseState                                                     m_MouseState;

    std::unordered_map<UserDefAction, std::variant<VirtualKeyCode, MouseEvent>> m_UserMap;
};

extern std::unique_ptr<InputManager> g_InputManager;
}  // namespace Hitagi