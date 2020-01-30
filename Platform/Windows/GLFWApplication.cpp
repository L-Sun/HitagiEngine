#include <iostream>
#include "GLFWApplication.hpp"

using namespace My;

int GLFWApplication::Initialize() {
    int result;

    glfwInit();
    for (auto hint : WindowHintConfig) {
        glfwWindowHint(hint.first, hint.second);
    }

    m_window = glfwCreateWindow(m_Config.screenWidth, m_Config.screenHeight,
                                m_Config.appName.c_str(), NULL, NULL);

    if (m_window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(m_window);

    // first call base class initialization
    result = BaseApplication::Initialize();
    if (result != 0) exit(result);

    return result;
}

void GLFWApplication::Finalize() { glfwTerminate(); }

void GLFWApplication::Tick() {
    BaseApplication::m_bQuit = glfwWindowShouldClose(m_window);
    BaseApplication::Tick();
    glfwSetKeyCallback(m_window, KeyCallback);
    glfwPollEvents();
    OnDraw();
    glfwSwapBuffers(m_window);
}

void GLFWApplication::KeyCallback(GLFWwindow* window, int key, int scancode,
                                  int action, int mods) {
    if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT))
        g_pInputManager->UpArrowKeyDown();
    else if (key == GLFW_KEY_UP && action == GLFW_RELEASE)
        g_pInputManager->UpArrowKeyUp();
    else if (key == GLFW_KEY_DOWN &&
             (action == GLFW_PRESS || action == GLFW_REPEAT))
        g_pInputManager->DownArrowKeyDown();
    else if (key == GLFW_KEY_DOWN && action == GLFW_RELEASE)
        g_pInputManager->DownArrowKeyUp();
    else if (key == GLFW_KEY_LEFT &&
             (action == GLFW_PRESS || action == GLFW_REPEAT))
        g_pInputManager->LeftArrowKeyDown();
    else if (key == GLFW_KEY_LEFT && action == GLFW_RELEASE)
        g_pInputManager->LeftArrowKeyUp();
    else if (key == GLFW_KEY_RIGHT &&
             (action == GLFW_PRESS || action == GLFW_REPEAT))
        g_pInputManager->RightArrowKeyDown();
    else if (key == GLFW_KEY_RIGHT && action == GLFW_RELEASE)
        g_pInputManager->RightArrowKeyUp();
    else if (key == GLFW_KEY_D &&
             (action == GLFW_PRESS || action == GLFW_REPEAT))
        g_pInputManager->DebugKeyDown();
    else if (key == GLFW_KEY_D && action == GLFW_RELEASE)
        g_pInputManager->DebugKeyUp();
    else if (key == GLFW_KEY_R && action == GLFW_PRESS)
        g_pInputManager->ResetKeyDown();
    else if (key == GLFW_KEY_ESCAPE &&
             (action == GLFW_PRESS || action == GLFW_REPEAT))
        glfwSetWindowShouldClose(window, true);
}
