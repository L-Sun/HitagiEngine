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

    void UpdateKeyState(VirtualKeyCode key, bool state) noexcept { m_KeyState[static_cast<size_t>(key)].current = state; }
    void UpdatePointerState(std::array<float, 2> position) noexcept { m_MouseState.curr_pos = position; }
    void UpdateWheelState(float delta) noexcept { m_MouseState.scroll = delta; }

    void Map(UserDefAction user_action, std::variant<VirtualKeyCode, MouseEvent> event);

    bool  GetBool(UserDefAction user_action) const;
    bool  GetBoolNew(UserDefAction user_action) const;
    float GetFloat(UserDefAction user_action) const;
    float GetFloatDelta(UserDefAction user_action) const;

private:
    std::array<KeyState, static_cast<size_t>(VirtualKeyCode::NUM)> m_KeyState;
    MouseState                                                     m_MouseState{};

    std::unordered_map<UserDefAction, std::variant<VirtualKeyCode, MouseEvent>> m_UserMap;
};

extern std::unique_ptr<InputManager> g_InputManager;
}  // namespace Hitagi