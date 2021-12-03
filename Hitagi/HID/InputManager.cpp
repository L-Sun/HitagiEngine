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
    for (auto&& state : m_KeyState)
        state.previous = state.current;

    m_MouseState.last_pos = m_MouseState.curr_pos;
    m_MouseState.scroll   = 0;
    m_TextInput.clear();

    g_App->UpdateInputEvent();
}

void InputManager::Map(std::string user_action, std::variant<VirtualKeyCode, MouseEvent> event) {
    if (m_UserMap.count(user_action) != 0) {
        std::visit(
            Overloaded{
                [&](const VirtualKeyCode& key) {
                    m_Logger->warn(
                        "the user action: [{} -> key:{}] exsited,"
                        " you are trying to override it!",
                        user_action, static_cast<int>(key));
                },
                [&](const MouseEvent& event) {
                    m_Logger->warn(
                        "the user action: [{} -> key:{}] exsited,"
                        " you are trying to override it!",
                        user_action, magic_enum::enum_name(event));
                },
            },
            m_UserMap.at(user_action));

        m_UserMap.at(user_action) = std::move(event);
    } else {
        m_UserMap.emplace(std::move(user_action), std::move(event));
    }
}

bool InputManager::HasAction(const std::string& user_action) const {
    if (m_UserMap.count(user_action) == 0) {
        m_Logger->error("you are trying to get unmap action: [{}]", user_action);
        m_Logger->error("HID will return zero value!!!");
        return false;
    }
    return true;
}

bool InputManager::GetBool(const std::string& user_action) const {
    return HasAction(user_action) ? GetBool(m_UserMap.at(user_action)) : false;
}
bool InputManager::GetBoolNew(const std::string& user_action) const {
    return HasAction(user_action) ? GetBoolNew(m_UserMap.at(user_action)) : false;
}
float InputManager::GetFloat(const std::string& user_action) const {
    return HasAction(user_action) ? GetFloat(m_UserMap.at(user_action)) : 0.0f;
}
float InputManager::GetFloatDelta(const std::string& user_action) const {
    return HasAction(user_action) ? GetFloatDelta(m_UserMap.at(user_action)) : 0.0f;
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
                        return (m_MouseState.curr_pos[0] - m_MouseState.last_pos[0]) != 0;
                    case MouseEvent::MOVE_Y:
                        return (m_MouseState.curr_pos[1] - m_MouseState.last_pos[1]) != 0;
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll != 0;
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
                        return (m_MouseState.curr_pos[0] - m_MouseState.last_pos[0]) != 0;
                    case MouseEvent::MOVE_Y:
                        return (m_MouseState.curr_pos[1] - m_MouseState.last_pos[1]) != 0;
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll != 0;
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
                        return m_MouseState.curr_pos[0];
                    case MouseEvent::MOVE_Y:
                        return m_MouseState.curr_pos[1];
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll;
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
                        return m_MouseState.curr_pos[0] - m_MouseState.last_pos[0];
                    case MouseEvent::MOVE_Y:
                        return m_MouseState.curr_pos[1] - m_MouseState.last_pos[1];
                    case MouseEvent::SCROLL:
                        return m_MouseState.scroll;
                }
            },
        },
        event);
}

}  // namespace Hitagi