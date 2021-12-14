#pragma once
#include "IRuntimeModule.hpp"
#include "InputEvent.hpp"

#include <variant>
#include <optional>
#include <unordered_map>

namespace Hitagi {

class InputManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void UpdateKeyState(VirtualKeyCode key, bool state) noexcept { m_KeyState[static_cast<size_t>(key)].current = state; }
    void UpdatePointerState(std::array<float, 2> position) noexcept { m_MouseState.curr_pos = position; }
    void UpdateWheelState(float delta) noexcept { m_MouseState.scroll = delta; }
    void AppendInputText(std::u8string text) noexcept { m_TextInput.append(std::move(text)); }

    bool  GetBool(std::variant<VirtualKeyCode, MouseEvent> event) const;
    bool  GetBoolNew(std::variant<VirtualKeyCode, MouseEvent> event) const;
    float GetFloat(std::variant<VirtualKeyCode, MouseEvent> event) const;
    float GetFloatDelta(std::variant<VirtualKeyCode, MouseEvent> event) const;

    const std::u8string& GetInputText() const noexcept { return m_TextInput; };

private:
    std::array<KeyState, static_cast<size_t>(VirtualKeyCode::NUM)> m_KeyState;
    MouseState                                                     m_MouseState{};
    std::u8string                                                  m_TextInput;
};

extern std::unique_ptr<InputManager> g_InputManager;
}  // namespace Hitagi