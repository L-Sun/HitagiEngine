#include "InputManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "Application.hpp"
#include "portable.hpp"

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

    m_MouseState.lastPos = m_MouseState.currPos;

    g_App->UpdateInputEvent();
}

void InputManager::Map(UserDefAction userAction, std::variant<VirtualKeyCode, MouseEvent> event) {
    m_UserMap[userAction] = std::move(event);
}

bool InputManager::GetBool(UserDefAction userAction) const {
    return std::visit(
        [this](auto& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, VirtualKeyCode>)
                return m_KeyState[static_cast<size_t>(arg)].current;
            else if constexpr (std::is_same_v<T, MouseEvent>) {
                switch (arg) {
                    case MouseEvent::MOVE_X:
                        return (m_MouseState.currPos[0] - m_MouseState.lastPos[0]) != 0;
                    case MouseEvent::MOVE_Y:
                        return (m_MouseState.currPos[1] - m_MouseState.lastPos[1]) != 0;
                    case MouseEvent::SCROLL_X:
                        return (m_MouseState.currScroll[0] - m_MouseState.lastScroll[0]) != 0;
                    case MouseEvent::SCROLL_Y:
                        return (m_MouseState.currScroll[1] - m_MouseState.lastScroll[1]) != 0;
                }
            } else
                static_assert(always_false_v<T>, "non-exhaustive visitor!");
            return false;
        },
        m_UserMap.at(userAction));
}

bool InputManager::GetBoolNew(UserDefAction userAction) const {
    return std::visit(
        [this](auto& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, VirtualKeyCode>)
                return m_KeyState[static_cast<size_t>(arg)].current && !m_KeyState[static_cast<size_t>(arg)].previous;
            else if constexpr (std::is_same_v<T, MouseEvent>) {
                switch (arg) {
                    case MouseEvent::MOVE_X:
                        return (m_MouseState.currPos[0] - m_MouseState.lastPos[0]) != 0;
                    case MouseEvent::MOVE_Y:
                        return (m_MouseState.currPos[1] - m_MouseState.lastPos[1]) != 0;
                    case MouseEvent::SCROLL_X:
                        return (m_MouseState.currScroll[0] - m_MouseState.lastScroll[0]) != 0;
                    case MouseEvent::SCROLL_Y:
                        return (m_MouseState.currScroll[1] - m_MouseState.lastScroll[1]) != 0;
                }
            } else
                static_assert(always_false_v<T>, "non-exhaustive visitor!");
            return false;
        },
        m_UserMap.at(userAction));
}

float InputManager::GetFloat(UserDefAction userAction) const {
    return std::visit(
        [this](auto& arg) -> float {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, VirtualKeyCode>)
                return m_KeyState[static_cast<size_t>(arg)].current ? 1 : 0;
            else if constexpr (std::is_same_v<T, MouseEvent>) {
                switch (arg) {
                    case MouseEvent::MOVE_X:
                        return m_MouseState.currPos[0];
                    case MouseEvent::MOVE_Y:
                        return m_MouseState.currPos[1];
                    case MouseEvent::SCROLL_X:
                        return m_MouseState.currScroll[0];
                    case MouseEvent::SCROLL_Y:
                        return m_MouseState.currScroll[1];
                }
            } else
                static_assert(always_false_v<T>, "non-exhaustive visitor!");
            return 0;
        },
        m_UserMap.at(userAction));
}

float InputManager::GetFloatDelta(UserDefAction userAction) const {
    return std::visit(
        [this](auto& arg) -> float {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, VirtualKeyCode>)
                return (m_KeyState[static_cast<size_t>(arg)].current &&
                        !m_KeyState[static_cast<size_t>(arg)].previous)
                           ? 1
                           : 0;
            else if constexpr (std::is_same_v<T, MouseEvent>) {
                switch (arg) {
                    case MouseEvent::MOVE_X:
                        return m_MouseState.currPos[0] - m_MouseState.lastPos[0];
                    case MouseEvent::MOVE_Y:
                        return m_MouseState.currPos[1] - m_MouseState.lastPos[1];
                    case MouseEvent::SCROLL_X:
                        return m_MouseState.currScroll[0] - m_MouseState.lastScroll[0];
                    case MouseEvent::SCROLL_Y:
                        return m_MouseState.currScroll[1] - m_MouseState.lastScroll[1];
                }
            } else
                static_assert(always_false_v<T>, "non-exhaustive visitor!");
            return 0;
        },
        m_UserMap.at(userAction));
}

}  // namespace Hitagi