#include <iostream>
#include "InputManager.hpp"
#include "SceneManager.hpp"
#include "DebugManager.hpp"

using namespace Hitagi;

#define DEBUG 1

int  InputManager::Initialize() { return 0; }
void InputManager::Finalize() {}
void InputManager::Tick() {}

void InputManager::UpArrowKeyDown() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Up Arrow Key Down!" << std::endl;
#endif
    if (!m_UpKeyPressed) {
        m_UpKeyPressed = true;
    }
}

void InputManager::UpArrowKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Up Arrow Key Up!" << std::endl;
#endif
    m_UpKeyPressed = false;
}

void InputManager::DownArrowKeyDown() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Down Arrow Key Down!" << std::endl;
#endif
    if (!m_DownKeyPressed) {
        m_DownKeyPressed = true;
    }
}

void InputManager::DownArrowKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Down Arrow Key Up!" << std::endl;
#endif
    m_DownKeyPressed = false;
}

void InputManager::LeftArrowKeyDown() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Left Arrow Key Down!" << std::endl;
#endif
    if (!m_LeftKeyPressed) {
        m_LeftKeyPressed = true;
    }
}

void InputManager::LeftArrowKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Left Arrow Key Up!" << std::endl;
#endif
    m_LeftKeyPressed = false;
}

void InputManager::RightArrowKeyDown() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Right Arrow Key Down!" << std::endl;
#endif
    if (!m_RightKeyPressed) {
        m_RightKeyPressed = true;
    }
}

void InputManager::RightArrowKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Right Arrow Key Up!" << std::endl;
#endif
    m_RightKeyPressed = false;
}

void InputManager::CKeyDown() {
#if defined(_DEBUG)
    std::cout << "[InputManager] C Key Down!" << std::endl;
#endif
    if (!m_RightKeyPressed) {
        m_CKeyPressed = true;
    }
}

void InputManager::CKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] C Key Up!" << std::endl;
#endif
    m_CKeyPressed = false;
}

void InputManager::ResetKeyDown() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Reset Key Down!" << std::endl;
#endif
    g_SceneManager->ResetScene();
}

void InputManager::ResetKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Reset Key Up!" << std::endl;
#endif
}

#if defined(_DEBUG)
void InputManager::DebugKeyDown() {
    std::cout << "[InputManager] Debug Key Down!" << std::endl;
    g_DebugManager->ToggleDebugInfo();
}
#endif

#if defined(_DEBUG)
void InputManager::DebugKeyUp() { std::cout << "[InputManager] Debug Key Up!" << std::endl; }
#endif