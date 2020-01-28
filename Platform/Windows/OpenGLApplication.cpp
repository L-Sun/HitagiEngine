// the fllowing include order can not change, because the header glad.h must be
// before glwf3.h
#include "OpenGLGraphicsManager.hpp"
#include "OpenGLApplication.hpp"

using namespace My;

int OpenGLApplication::Initialize() {
    int result;
    result = GLFWApplication::Initialize();
    if (result != 0) exit(result);
    result = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    if (!result) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    } else {
        result = 0;
    }
    glViewport(0, 0, m_Config.screenWidth, m_Config.screenHeight);
    return result;
}

void OpenGLApplication::Finalize() { GLFWApplication::Finalize(); }
void OpenGLApplication::Tick() {
    g_pGraphicsManager->Clear();
    g_pGraphicsManager->Draw();
    GLFWApplication::Tick();
}
