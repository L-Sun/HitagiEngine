#include "InputManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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
    for (auto&& state : m_KeyState)
        state.previous = state.current;

    m_MouseState.last_pos = m_MouseState.curr_pos;
    m_MouseState.scroll   = 0;

    g_App->UpdateInputEvent();
}

void InputManager::Map(UserDefAction user_action, std::variant<VirtualKeyCode, MouseEvent> event) {
    m_UserMap[user_action] = std::move(event);
}

bool InputManager::GetBool(UserDefAction user_action) const {
    return std::visit(
        Overloaded{
            [&](const VirtualKeyCode& key) -> bool {
                return m_KeyState[static_cast<size_t>(key)].current;
            },
            [&](const MouseEvent& event) -> bool {
                switch (event) {
                    case MouseEvent::MOVE_X:
                        return (m_MouseState.curr_pos[0] - m_MouseState.last_pos[0]) != 0;
                    case MouseEvent::MOVE_Y:
                        return (m_MouseState.curr_pos[1] - m_MouseState.last_pos[1]) != 0;
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll != 0;
                }
            },
        },
        m_UserMap.at(user_action));
}

bool InputManager::GetBoolNew(UserDefAction user_action) const {
    return std::visit(
        Overloaded{
            [&](const VirtualKeyCode& key) -> bool {
                return m_KeyState[static_cast<size_t>(key)].current && !m_KeyState[static_cast<size_t>(key)].previous;
            },
            [&](const MouseEvent& event) -> bool {
                switch (event) {
                    case MouseEvent::MOVE_X:
                        return (m_MouseState.curr_pos[0] - m_MouseState.last_pos[0]) != 0;
                    case MouseEvent::MOVE_Y:
                        return (m_MouseState.curr_pos[1] - m_MouseState.last_pos[1]) != 0;
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll != 0;
                }
            },
        },
        m_UserMap.at(user_action));
}

float InputManager::GetFloat(UserDefAction user_action) const {
    return std::visit(
        Overloaded{
            [&](const VirtualKeyCode& key) -> float {
                return m_KeyState[static_cast<size_t>(key)].current ? 1.0f : 0.0f;
            },
            [&](const MouseEvent& event) -> float {
                switch (event) {
                    case MouseEvent::MOVE_X:
                        return m_MouseState.curr_pos[0];
                    case MouseEvent::MOVE_Y:
                        return m_MouseState.curr_pos[1];
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll;
                }
            },
        },
        m_UserMap.at(user_action));
}

float InputManager::GetFloatDelta(UserDefAction user_action) const {
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
                        return m_MouseState.curr_pos[0] - m_MouseState.last_pos[0];
                    case MouseEvent::MOVE_Y:
                        return m_MouseState.curr_pos[1] - m_MouseState.last_pos[1];
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll;
                }
            },
        },
        m_UserMap.at(user_action));
}

}  // namespace Hitagi