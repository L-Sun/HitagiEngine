#include <iostream>
#include "MemoryManager.hpp"
#include "OpenGL/OpenGLGraphicsManager.hpp"
#include "GLFWApplication.hpp"

using namespace My;

int GLFWApplication::Initialize() {
    int result;

    // first call base class initialization
    result = BaseApplication::Initialize();

    if (result != 0) exit(result);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(m_Config.screenWidth, m_Config.screenHeight,
                                m_Config.appName, NULL, NULL);

    if (m_window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(m_window);
    return result;
}

void GLFWApplication::Finalize() { glfwTerminate(); }

void GLFWApplication::Tick() {
    BaseApplication::m_bQuit = glfwWindowShouldClose(m_window);
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(m_window, true);
    glfwPollEvents();
    OnDraw();
    glfwSwapBuffers(m_window);
}
