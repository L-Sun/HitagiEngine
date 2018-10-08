#include "OpenGL/OpenGLGraphicsManager.hpp"
#include "MemoryManager.hpp"
#include "OpenGLApplication.hpp"

using namespace My;

namespace My {
GfxConfiguration config("Game Engine From Scratch (Linux)", 8, 8, 8, 8, 24, 8,
                        0, 960, 540);
IApplication*    g_pApp =
    static_cast<IApplication*>(new OpenGLApplication(config));
GraphicsManager* g_pGraphicsManager =
    static_cast<GraphicsManager*>(new OpenGLGraphicsManager);
MemoryManager* g_pMemoryManager =
    static_cast<MemoryManager*>(new MemoryManager);
}  // namespace My

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
void OpenGLApplication::Tick() { GLFWApplication::Tick(); }
void OpenGLApplication::OnDraw() {
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}