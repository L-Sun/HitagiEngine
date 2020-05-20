#include "InputManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "IApplication.hpp"

namespace Hitagi {

std::unique_ptr<InputManager> g_InputManager = std::make_unique<InputManager>();

int InputManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("InputManager");
    m_Logger->info("Initialize...");

#if defined(DEBUG)
    m_Logger->set_level(spdlog::level::debug);
#endif  // DEBUG

    auto& config = g_App->GetConfiguration();

    return 0;
}

void InputManager::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}
void InputManager::Tick() {
    for (auto&& m : m_BoolMapping) {
        m.previous = m.current;
        m.current  = false;  // will update in UpdateInputState()
    }
    for (auto&& m : m_FloatMapping) {
        m.previous = m.current;
        m.current  = 0.0f;  // will update in UpdateInputState()
    }

    // Update state
    g_App->UpdateInputState();
}

void InputManager::Map(UserDefAction userAction, InputEvent key) {
    if (userAction >= m_UserMapping.size()) m_UserMapping.resize(userAction + 1);
    m_UserMapping[userAction] = key;
}

bool InputManager::GetBool(UserDefAction userAction) const {
    return m_BoolMapping[static_cast<unsigned>(m_UserMapping[userAction])].current;
}

bool InputManager::GetBoolNew(UserDefAction userAction) const {
    auto& state = m_BoolMapping[static_cast<unsigned>(m_UserMapping[userAction])];
    return state.current && !state.previous;
}

float InputManager::GetFloat(UserDefAction userAction) const {
    return m_FloatMapping[static_cast<unsigned>(m_UserMapping[userAction])].current;
}

float InputManager::GetFloatDelta(UserDefAction userAction) const {
    auto& state = m_FloatMapping[static_cast<unsigned>(m_UserMapping[userAction])];
    return state.current - state.previous;
}

}  // namespace Hitagi