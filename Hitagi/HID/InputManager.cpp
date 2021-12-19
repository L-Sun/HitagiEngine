#include "InputManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <magic_enum.hpp>

#include "Application.hpp"

template <class... Ts>
struct Overloaded : Ts... { using Ts::operator()...; };
// 显式推导指引（ C++20 起不需要）
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

namespace Hitagi {

std::unique_ptr<InputManager> g_InputManager = std::make_unique<InputManager>();

int InputManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("InputManager");
    m_Logger->info("Initialize...");

    auto& config = g_App->GetConfiguration();

    return 0;
}

void InputManager::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}
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
}

bool InputManager::GetBool(std::variant<VirtualKeyCode, MouseEvent> event) const {
    return std::visit(
        Overloaded{
            [&](const VirtualKeyCode& key) -> bool {
                return m_KeyState[static_cast<size_t>(key)].current;
            },
            [&](const MouseEvent& event) -> bool {
                switch (event) {
                    case MouseEvent::MOVE_X:
                        return (m_MouseState.position.current[0] - m_MouseState.position.previous[0]) != 0;
                    case MouseEvent::MOVE_Y:
                        return (m_MouseState.position.current[1] - m_MouseState.position.previous[1]) != 0;
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll.current != 0;
                }
            },
        },
        event);
}

bool InputManager::GetBoolNew(std::variant<VirtualKeyCode, MouseEvent> event) const {
    return std::visit(
        Overloaded{
            [&](const VirtualKeyCode& key) -> bool {
                return m_KeyState[static_cast<size_t>(key)].current && !m_KeyState[static_cast<size_t>(key)].previous;
            },
            [&](const MouseEvent& event) -> bool {
                switch (event) {
                    case MouseEvent::MOVE_X:
                        return (m_MouseState.position.current[0] - m_MouseState.position.previous[0]) != 0;
                    case MouseEvent::MOVE_Y:
                        return (m_MouseState.position.current[1] - m_MouseState.position.previous[1]) != 0;
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll.current != m_MouseState.scroll.previous;
                }
            },
        },
        event);
}

float InputManager::GetFloat(std::variant<VirtualKeyCode, MouseEvent> event) const {
    return std::visit(
        Overloaded{
            [&](const VirtualKeyCode& key) -> float {
                return m_KeyState[static_cast<size_t>(key)].current ? 1.0f : 0.0f;
            },
            [&](const MouseEvent& event) -> float {
                switch (event) {
                    case MouseEvent::MOVE_X:
                        return m_MouseState.position.current[0];
                    case MouseEvent::MOVE_Y:
                        return m_MouseState.position.current[1];
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll.current;
                }
            },
        },
        event);
}

float InputManager::GetFloatDelta(std::variant<VirtualKeyCode, MouseEvent> event) const {
    return std::visit(
        Overloaded{
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
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll.current - m_MouseState.scroll.previous;
                }
            },
        },
        event);
}

}  // namespace Hitagi