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

    constexpr std::array keyToGlfw = {
        std::make_pair(InputEvent::KEY_R, GLFW_KEY_R),
        std::make_pair(InputEvent::KEY_W, GLFW_KEY_W),
        std::make_pair(InputEvent::KEY_A, GLFW_KEY_A),
        std::make_pair(InputEvent::KEY_S, GLFW_KEY_S),
        std::make_pair(InputEvent::KEY_D, GLFW_KEY_D),
        std::make_pair(InputEvent::KEY_SPACE, GLFW_KEY_SPACE),
        std::make_pair(InputEvent::KEY_UP, GLFW_KEY_UP),
        std::make_pair(InputEvent::KEY_DOWN, GLFW_KEY_DOWN),
        std::make_pair(InputEvent::KEY_LEFT, GLFW_KEY_LEFT),
        std::make_pair(InputEvent::KEY_RIGHT, GLFW_KEY_RIGHT),
    };
    for (auto&& [event, glfwKey] : keyToGlfw) {
        const auto index            = static_cast<unsigned>(event);
        boolMapping[index].current  = glfwGetKey(m_Window, glfwKey) != GLFW_RELEASE;
        floatMapping[index].current = boolMapping[index].current ? 1.f : 0.f;
    }

    // Process Mouse Input
    constexpr std::array buttonToGlfw{
        std::make_pair(InputEvent::MOUSE_LEFT, GLFW_MOUSE_BUTTON_LEFT),
        std::make_pair(InputEvent::MOUSE_RIGHT, GLFW_MOUSE_BUTTON_RIGHT),
        std::make_pair(InputEvent::MOUSE_MIDDLE, GLFW_MOUSE_BUTTON_MIDDLE),
    };
    for (auto&& [event, glfwButton] : buttonToGlfw) {
        const auto index            = static_cast<unsigned>(event);
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