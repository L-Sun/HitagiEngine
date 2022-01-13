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

    inline void UpdateKeyState(VirtualKeyCode key, bool state) noexcept {
        m_KeyState[static_cast<size_t>(key)].Update(state);
    }
    inline void UpdatePointerState(std::array<float, 2> position) noexcept {
        m_MouseState.position.Update(position);
    }
    inline void UpdateWheelState(float delta) noexcept {
        m_MouseState.scroll.Update(m_MouseState.scroll.current + delta);
    }
    inline void AppendInputText(std::u32string text) noexcept {
        m_TextInput.append(std::move(text));
        m_TextInputDirty = true;
    }

    bool  GetBool(std::variant<VirtualKeyCode, MouseEvent> event) const;
    bool  GetBoolNew(std::variant<VirtualKeyCode, MouseEvent> event) const;
    float GetFloat(std::variant<VirtualKeyCode, MouseEvent> event) const;
    float GetFloatDelta(std::variant<VirtualKeyCode, MouseEvent> event) const;

    const std::u32string& GetInputText() const noexcept { return m_TextInput; };

private:
    std::array<KeyState, static_cast<size_t>(VirtualKeyCode::NUM)> m_KeyState;
    MouseState                                                     m_MouseState{};
    std::u32string                                                 m_TextInput;
    bool                                                           m_TextInputDirty = false;
};

extern std::unique_ptr<InputManager> g_InputManager;
}  // namespace Hitagi