#include "GLFWApplication.hpp"

#include <iostream>

#include <spdlog/spdlog.h>

#include "InputManager.hpp"

using namespace Hitagi;

int GLFWApplication::Initialize() {
    if (!glfwInit()) {
        m_Logger->error("GLFW3 initialize failed.");
        return -1;
    }

    m_Window =
        glfwCreateWindow(m_Config.screenWidth, m_Config.screenHeight, m_Config.appName.c_str(), nullptr, nullptr);
    if (!m_Window) {
        m_Logger->error("Create Window failed.");
        glfwTerminate();
        return -1;
    }
    glfwSetScrollCallback(m_Window, ProcessScroll);
    // first call base class initialization
    return BaseApplication::Initialize();
}

void GLFWApplication::Finalize() {
    if (m_Window) glfwDestroyWindow(m_Window);
    glfwTerminate();
    BaseApplication::Finalize();
}

void GLFWApplication::Tick() {
    BaseApplication::Tick();
    BaseApplication::m_Quit = glfwWindowShouldClose(m_Window);
}

void GLFWApplication::UpdateInputState() {
    // If scroll wheel, it will callback ProcessScroll function.
    glfwPollEvents();

    // Process Key Input
    auto& boolMapping  = g_InputManager->m_BoolMapping;
    auto& floatMapping = g_InputManager->m_FloatMapping;

    constexpr std::pair<InputEvent, int> keyToGlfw[] = {
        {InputEvent::KEY_D, GLFW_KEY_D},
    };
    for (auto&& [event, glfwKey] : keyToGlfw) {
        const unsigned index        = static_cast<unsigned>(event);
        boolMapping[index].current  = glfwGetKey(m_Window, glfwKey) != GLFW_RELEASE;
        floatMapping[index].current = boolMapping[index].current ? 1.f : 0.f;
    }

    // Process Mouse Input
    constexpr std::pair<InputEvent, int> buttonToGlfw[] = {
        {InputEvent::MOUSE_LEFT, GLFW_MOUSE_BUTTON_LEFT},
        {InputEvent::MOUSE_RIGHT, GLFW_MOUSE_BUTTON_RIGHT},
        {InputEvent::MOUSE_MIDDLE, GLFW_MOUSE_BUTTON_MIDDLE},
    };
    for (auto&& [event, glfwButton] : buttonToGlfw) {
        const unsigned index        = static_cast<unsigned>(event);
        boolMapping[index].current  = glfwGetMouseButton(m_Window, glfwButton) != GLFW_RELEASE;
        floatMapping[index].current = boolMapping[index].current ? 1.f : 0.f;
    }

    double x, y;
    glfwGetCursorPos(m_Window, &x, &y);
    boolMapping[static_cast<unsigned>(InputEvent::MOUSE_MOVE_X)].current  = (x != 0);
    boolMapping[static_cast<unsigned>(InputEvent::MOUSE_MOVE_Y)].current  = (y != 0);
    floatMapping[static_cast<unsigned>(InputEvent::MOUSE_MOVE_X)].current = x;
    floatMapping[static_cast<unsigned>(InputEvent::MOUSE_MOVE_Y)].current = y;
}

void GLFWApplication::ProcessScroll(GLFWwindow* window, double xoffset, double yoffset) {
    auto& boolMapping  = g_InputManager->m_BoolMapping;
    auto& floatMapping = g_InputManager->m_FloatMapping;

    boolMapping[static_cast<unsigned>(InputEvent::MOUSE_SCROLL_X)].current  = (xoffset != 0);
    boolMapping[static_cast<unsigned>(InputEvent::MOUSE_SCROLL_Y)].current  = (yoffset != 0);
    floatMapping[static_cast<unsigned>(InputEvent::MOUSE_SCROLL_X)].current = xoffset;
    floatMapping[static_cast<unsigned>(InputEvent::MOUSE_SCROLL_Y)].current = yoffset;
}