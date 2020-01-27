#include <iostream>
#include "InputManager.hpp"
#include "GraphicsManager.hpp"
#include "SceneManager.hpp"
#include "GameLogic.hpp"
#include "DebugManager.hpp"
#include "geommath.hpp"

using namespace My;

#define DEBUG 1

int  InputManager::Initialize() { return 0; }
void InputManager::Finalize() {}
void InputManager::Tick() {}

void InputManager::UpArrowKeyDown() {
#ifdef DEBUG
    std::cout << "[InputManager] Up Arrow Key Down!" << std::endl;
#endif
    g_pGameLogic->OnUpKeyDown();
    if (!m_bUpKeyPressed) {
        g_pGameLogic->OnUpKey();
        m_bUpKeyPressed = true;
    }
}

void InputManager::UpArrowKeyUp() {
#ifdef DEBUG
    std::cout << "[InputManager] Up Arrow Key Up!" << std::endl;
#endif
    g_pGameLogic->OnUpKeyUp();
    m_bUpKeyPressed = false;
}

void InputManager::DownArrowKeyDown() {
#ifdef DEBUG
    std::cout << "[InputManager] Down Arrow Key Down!" << std::endl;
#endif
    g_pGameLogic->OnDownKeyDown();
    if (!m_bDownKeyPressed) {
        g_pGameLogic->OnDownKey();
        m_bDownKeyPressed = true;
    }
}

void InputManager::DownArrowKeyUp() {
#ifdef DEBUG
    std::cout << "[InputManager] Down Arrow Key Up!" << std::endl;
#endif
    g_pGameLogic->OnDownKeyUp();
    m_bDownKeyPressed = false;
}

void InputManager::LeftArrowKeyDown() {
#ifdef DEBUG
    std::cout << "[InputManager] Left Arrow Key Down!" << std::endl;
#endif
    g_pGameLogic->OnLeftKeyDown();
    if (!m_bLeftKeyPressed) {
        g_pGameLogic->OnLeftKey();
        m_bLeftKeyPressed = true;
    }
}

void InputManager::LeftArrowKeyUp() {
#ifdef DEBUG
    std::cout << "[InputManager] Left Arrow Key Up!" << std::endl;
#endif
    g_pGameLogic->OnLeftKeyUp();
    m_bLeftKeyPressed = false;
}

void InputManager::RightArrowKeyDown() {
#ifdef DEBUG
    std::cout << "[InputManager] Right Arrow Key Down!" << std::endl;
#endif
    g_pGameLogic->OnRightKeyDown();
    if (!m_bRightKeyPressed) {
        g_pGameLogic->OnRightKey();
        m_bRightKeyPressed = true;
    }
}

void InputManager::RightArrowKeyUp() {
#ifdef DEBUG
    std::cout << "[InputManager] Right Arrow Key Up!" << std::endl;
#endif
    g_pGameLogic->OnRightKeyUp();
    m_bRightKeyPressed = false;
}

void InputManager::CKeyDown() {
#ifdef DEBUG
    std::cout << "[InputManager] C Key Down!" << std::endl;
#endif
    g_pGameLogic->OnCKeyDown();
    if (!m_bRightKeyPressed) {
        g_pGameLogic->OnCKey();
        m_bCKeyPressed = true;
    }
}

void InputManager::CKeyUp() {
#ifdef DEBUG
    std::cout << "[InputManager] C Key Up!" << std::endl;
#endif
    g_pGameLogic->OnCKeyUp();
    m_bCKeyPressed = false;
}

void InputManager::ResetKeyDown() {
#ifdef DEBUG
    std::cout << "[InputManager] Reset Key Down!" << std::endl;
#endif
    g_pSceneManager->ResetScene();
}

void InputManager::ResetKeyUp() {
#ifdef DEBUG
    std::cout << "[InputManager] Reset Key Up!" << std::endl;
#endif
}

#ifdef DEBUG
void InputManager::DebugKeyDown() {
    std::cout << "[InputManager] Debug Key Down!" << std::endl;
    g_pDebugManager->ToggleDebugInfo();
}
#endif

#ifdef DEBUG
void InputManager::DebugKeyUp() {
    std::cout << "[InputManager] Debug Key Up!" << std::endl;
}
#endif