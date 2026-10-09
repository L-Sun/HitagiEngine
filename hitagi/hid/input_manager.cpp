export module hid:input_manager;
import interop.magic_enum;

import std;
import utils;
import math;
import core;

import :types;

export namespace hitagi::hid {

class InputManager : public core::RuntimeModule {
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
    void ResetInputState() noexcept;

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

namespace hitagi::hid {
InputManager::InputManager()
    : core::RuntimeModule("InputManager"),
      m_KeyState(utils::create_array<KeyState, static_cast<std::size_t>(VirtualKeyCode::NUM)>(
          KeyState{
              .current  = false,
              .previous = false,
              .dirty    = false,
          })) {}

void InputManager::Tick() {
    for (auto&& state : m_KeyState) {
        state.ClearDirty();
    }

    m_MouseState.position.ClearDirty();
    m_MouseState.scroll.ClearDirty();

    if (m_TextInputDirty) {
        m_TextInputDirty = false;
    } else {
        m_TextInput.clear();
    }
    core::RuntimeModule::Tick();
}

void InputManager::ResetInputState() noexcept {
    for (auto&& state : m_KeyState) {
        state.current  = false;
        state.previous = false;
        state.dirty    = false;
    }

    m_MouseState.position.previous = m_MouseState.position.current;
    m_MouseState.position.dirty    = false;
    m_MouseState.scroll.current    = {};
    m_MouseState.scroll.previous   = {};
    m_MouseState.scroll.dirty      = false;
    m_TextInput.clear();
    m_TextInputDirty = false;
}

bool InputManager::GetBool(std::variant<VirtualKeyCode, MouseEvent> event) const {
    return std::visit(
        utils::Overloaded{
            [&](const VirtualKeyCode& key) -> bool {
                return m_KeyState[static_cast<size_t>(key)].current;
            },
            [&](const MouseEvent& event) -> bool {
                switch (event) {
                    case MouseEvent::MOVE_X:
                        return (m_MouseState.position.current[0] - m_MouseState.position.previous[0]) != 0;
                    case MouseEvent::MOVE_Y:
                        return (m_MouseState.position.current[1] - m_MouseState.position.previous[1]) != 0;
                    case MouseEvent::SCROLL_X:
                        return m_MouseState.scroll.current.x != 0;
                    case MouseEvent::SCROLL_Y:
                        return m_MouseState.scroll.current.y != 0;
                }
                throw std::logic_error(std::format("unimpletement mouse event: {}", magic_enum::enum_name(event)));
            },
        },
        event);
}

bool InputManager::GetBoolNew(std::variant<VirtualKeyCode, MouseEvent> event) const {
    return std::visit(
        utils::Overloaded{
            [&](const VirtualKeyCode& key) -> bool {
                return m_KeyState[static_cast<size_t>(key)].current && !m_KeyState[static_cast<size_t>(key)].previous;
            },
            [&](const MouseEvent& event) -> bool {
                switch (event) {
                    case MouseEvent::MOVE_X:
                        return (m_MouseState.position.current[0] - m_MouseState.position.previous[0]) != 0;
                    case MouseEvent::MOVE_Y:
                        return (m_MouseState.position.current[1] - m_MouseState.position.previous[1]) != 0;
                    case MouseEvent::SCROLL_X:
                        return m_MouseState.scroll.current.x != m_MouseState.scroll.previous.x;
                    case MouseEvent::SCROLL_Y:
                        return m_MouseState.scroll.current.y != m_MouseState.scroll.previous.y;
                }
                throw std::logic_error(std::format("unimpletement mouse event: {}", magic_enum::enum_name(event)));
            },
        },
        event);
}

float InputManager::GetFloat(std::variant<VirtualKeyCode, MouseEvent> event) const {
    return std::visit(
        utils::Overloaded{
            [&](const VirtualKeyCode& key) -> float {
                return m_KeyState[static_cast<size_t>(key)].current ? 1.0f : 0.0f;
            },
            [&](const MouseEvent& event) -> float {
                switch (event) {
                    case MouseEvent::MOVE_X:
                        return m_MouseState.position.current[0];
                    case MouseEvent::MOVE_Y:
                        return m_MouseState.position.current[1];
                    case MouseEvent::SCROLL_X:
                        return m_MouseState.scroll.current.x;
                    case MouseEvent::SCROLL_Y:
                        return m_MouseState.scroll.current.y;
                }
                throw std::logic_error(std::format("unimpletement mouse event: {}", magic_enum::enum_name(event)));
            },
        },
        event);
}

float InputManager::GetFloatDelta(std::variant<VirtualKeyCode, MouseEvent> event) const {
    return std::visit(
        utils::Overloaded{
            [&](const VirtualKeyCode& key) -> float {
                return (m_KeyState[static_cast<size_t>(key)].current &&
                        !m_KeyState[static_cast<size_t>(key)].previous)
                           ? 1.0f
                           : 0.0f;
            },
            [&](const MouseEvent& event) -> float {
                switch (event) {
                    case MouseEvent::MOVE_X:
                        return m_MouseState.position.current[0] - m_MouseState.position.previous[0];
                    case MouseEvent::MOVE_Y:
                        return m_MouseState.position.current[1] - m_MouseState.position.previous[1];
                    case MouseEvent::SCROLL_X:
                        return m_MouseState.scroll.current.x - m_MouseState.scroll.previous.x;
                    case MouseEvent::SCROLL_Y:
                        return m_MouseState.scroll.current.y - m_MouseState.scroll.previous.y;
                }
                throw std::logic_error(std::format("unimpletement mouse event: {}", magic_enum::enum_name(event)));
            },
        },
        event);
}

}  // namespace hitagi::hid
