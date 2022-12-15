#pragma once
#include <hitagi/hid/input_event.hpp>
#include <hitagi/core/runtime_module.hpp>

#include <string_view>
#include <variant>
#include <optional>
#include <unordered_map>

namespace hitagi::hid {

// TODO multiple window input handle
class InputManager : public RuntimeModule {
public:
    InputManager();
    void Tick() final;

    inline void UpdateKeyState(VirtualKeyCode key, bool state) noexcept {
        m_KeyState[static_cast<std::size_t>(key)].Update(state);
    }
    inline void UpdatePointerState(float x, float y) noexcept {
        m_MouseState.position.Update(math::vec2f{x, y});
    }
    inline void UpdateWheelState(float delta_v, float delta_h) noexcept {
        m_MouseState.scroll.Update(m_MouseState.scroll.current + math::vec2f{delta_v, delta_h});
    }
    inline void AppendInputText(const std::u32string& text) noexcept {
        m_TextInput.append(text);
        m_TextInputDirty = true;
    }

    bool  GetBool(std::variant<VirtualKeyCode, MouseEvent> event) const;
    bool  GetBoolNew(std::variant<VirtualKeyCode, MouseEvent> event) const;
    float GetFloat(std::variant<VirtualKeyCode, MouseEvent> event) const;
    float GetFloatDelta(std::variant<VirtualKeyCode, MouseEvent> event) const;

    const std::u32string& GetInputText() const noexcept { return m_TextInput; };

private:
    std::array<KeyState, static_cast<std::size_t>(VirtualKeyCode::NUM)> m_KeyState;
    MouseState                                                          m_MouseState{};
    std::u32string                                                      m_TextInput;
    bool                                                                m_TextInputDirty = false;
};

}  // namespace hitagi::hid

namespace hitagi {
extern hid::InputManager* input_manager;
}