#include <iostream>
#include "OpenGL/OpenGLGraphicsManager.hpp"
#include "GLFWApplication.hpp"

using namespace Hitagi;

int GLFWApplication::Initialize() {
    int result;

    glfwInit();
    for (auto [hint, value] : WindowHintConfig) {
        glfwWindowHint(hint, value);
    }

    m_Window = glfwCreateWindow(m_Config.screenWidth, m_Config.screenHeight, m_Config.appName.c_str(), NULL, NULL);

    if (m_Window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(m_Window);

    // first call base class initialization
    result = BaseApplication::Initialize();
    if (result != 0) exit(result);

    return result;
}

void GLFWApplication::Finalize() {
    glfwTerminate();
    BaseApplication::Finalize();
}

void GLFWApplication::Tick() {
    BaseApplication::m_Quit = glfwWindowShouldClose(m_Window);
    BaseApplication::Tick();
    glfwSetKeyCallback(m_Window, KeyCallback);
    glfwPollEvents();
    OnDraw();
    glfwSwapBuffers(m_Window);
}

void GLFWApplication::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT))
        ;
    else if (key == GLFW_KEY_UP && action == GLFW_RELEASE)
        ;
    else if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT))
        ;
    else if (key == GLFW_KEY_DOWN && action == GLFW_RELEASE)
        ;
    else if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT))
        ;
    else if (key == GLFW_KEY_LEFT && action == GLFW_RELEASE)
        ;
    else if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT))
        ;
    else if (key == GLFW_KEY_RIGHT && action == GLFW_RELEASE)
        ;
    else if (key == GLFW_KEY_D && (action == GLFW_PRESS || action == GLFW_REPEAT))
        ;
    else if (key == GLFW_KEY_D && action == GLFW_RELEASE)
        ;
    else if (key == GLFW_KEY_R && action == GLFW_PRESS)
        ;
    else if (key == GLFW_KEY_ESCAPE && (action == GLFW_PRESS || action == GLFW_REPEAT))
        glfwSetWindowShouldClose(window, true);
}
