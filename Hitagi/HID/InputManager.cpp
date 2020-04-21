#include "InputManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "SceneManager.hpp"
#include "DebugManager.hpp"

using namespace Hitagi;

int InputManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("InputManager");
    m_Logger->info("Initialize...");

#if defined(DEBUG)
    m_Logger->set_level(spdlog::level::debug);
#endif  // DEBUG
    return 0;
}
void InputManager::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}
void InputManager::Tick() {}

void InputManager::UpArrowKeyDown() {
    m_Logger->debug("Up Arrow Key Down!");
    if (!m_UpKeyPressed) {
        m_UpKeyPressed = true;
    }
}

void InputManager::UpArrowKeyUp() {
    m_Logger->debug("Up Arrow Key Up!");
    m_UpKeyPressed = false;
}

void InputManager::DownArrowKeyDown() {
    m_Logger->debug("Down Arrow Key Up!");
    if (!m_DownKeyPressed) {
        m_DownKeyPressed = true;
    }
}

void InputManager::DownArrowKeyUp() {
    m_Logger->debug("Down Arrow Key Up!");
    m_DownKeyPressed = false;
}

void InputManager::LeftArrowKeyDown() {
    m_Logger->debug("Left Arrow Key Down!");
    if (!m_LeftKeyPressed) {
        m_LeftKeyPressed = true;
    }
}

void InputManager::LeftArrowKeyUp() {
    m_Logger->debug("Left Arrow Key Up!");
    m_LeftKeyPressed = false;
}

void InputManager::RightArrowKeyDown() {
    m_Logger->debug("Left Arrow Key Up!");
    if (!m_RightKeyPressed) {
        m_RightKeyPressed = true;
    }
}

void InputManager::RightArrowKeyUp() {
    m_Logger->debug("Left Arrow Key Up!");
    m_RightKeyPressed = false;
}

void InputManager::CKeyDown() {
    m_Logger->debug("C Key Down!");
    if (!m_RightKeyPressed) {
        m_CKeyPressed = true;
    }
}

void InputManager::CKeyUp() {
    m_Logger->debug("C Key Up!");
    m_CKeyPressed = false;
}

void InputManager::ResetKeyDown() {
    m_Logger->debug("Reset  Key Down!");
    g_SceneManager->ResetScene();
}

void InputManager::ResetKeyUp() { m_Logger->debug("Reset Arrow Key Up!"); }

void InputManager::DebugKeyDown() {
    m_Logger->debug("Debug Key Down!");
    g_DebugManager->ToggleDebugInfo();
}

void InputManager::DebugKeyUp() { m_Logger->debug("Debug Key Up!"); }