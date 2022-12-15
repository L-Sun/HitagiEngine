#include <hitagi/hid/input_manager.hpp>
#include <hitagi/utils/overloaded.hpp>

#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

namespace hitagi {
hid::InputManager* input_manager = nullptr;
}

namespace hitagi::hid {
InputManager::InputManager()
    : RuntimeModule("InputManager"),
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
    RuntimeModule::Tick();
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
                throw std::logic_error(fmt::format("unimpletement mouse event: {}", magic_enum::enum_name(event)));
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
                throw std::logic_error(fmt::format("unimpletement mouse event: {}", magic_enum::enum_name(event)));
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
                throw std::logic_error(fmt::format("unimpletement mouse event: {}", magic_enum::enum_name(event)));
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
                throw std::logic_error(fmt::format("unimpletement mouse event: {}", magic_enum::enum_name(event)));
            },
        },
        event);
}

}  // namespace hitagi::hid