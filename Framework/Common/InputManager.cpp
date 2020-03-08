#include <iostream>
#include "InputManager.hpp"
#include "SceneManager.hpp"
#include "GameLogic.hpp"
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
    g_GameLogic->OnUpKeyDown();
    if (!m_UpKeyPressed) {
        g_GameLogic->OnUpKey();
        m_UpKeyPressed = true;
    }
}

void InputManager::UpArrowKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Up Arrow Key Up!" << std::endl;
#endif
    g_GameLogic->OnUpKeyUp();
    m_UpKeyPressed = false;
}

void InputManager::DownArrowKeyDown() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Down Arrow Key Down!" << std::endl;
#endif
    g_GameLogic->OnDownKeyDown();
    if (!m_DownKeyPressed) {
        g_GameLogic->OnDownKey();
        m_DownKeyPressed = true;
    }
}

void InputManager::DownArrowKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Down Arrow Key Up!" << std::endl;
#endif
    g_GameLogic->OnDownKeyUp();
    m_DownKeyPressed = false;
}

void InputManager::LeftArrowKeyDown() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Left Arrow Key Down!" << std::endl;
#endif
    g_GameLogic->OnLeftKeyDown();
    if (!m_LeftKeyPressed) {
        g_GameLogic->OnLeftKey();
        m_LeftKeyPressed = true;
    }
}

void InputManager::LeftArrowKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Left Arrow Key Up!" << std::endl;
#endif
    g_GameLogic->OnLeftKeyUp();
    m_LeftKeyPressed = false;
}

void InputManager::RightArrowKeyDown() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Right Arrow Key Down!" << std::endl;
#endif
    g_GameLogic->OnRightKeyDown();
    if (!m_RightKeyPressed) {
        g_GameLogic->OnRightKey();
        m_RightKeyPressed = true;
    }
}

void InputManager::RightArrowKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] Right Arrow Key Up!" << std::endl;
#endif
    g_GameLogic->OnRightKeyUp();
    m_RightKeyPressed = false;
}

void InputManager::CKeyDown() {
#if defined(_DEBUG)
    std::cout << "[InputManager] C Key Down!" << std::endl;
#endif
    g_GameLogic->OnCKeyDown();
    if (!m_RightKeyPressed) {
        g_GameLogic->OnCKey();
        m_CKeyPressed = true;
    }
}

void InputManager::CKeyUp() {
#if defined(_DEBUG)
    std::cout << "[InputManager] C Key Up!" << std::endl;
#endif
    g_GameLogic->OnCKeyUp();
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