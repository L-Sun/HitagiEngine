#include "glad/glad.h"
#include "OpenGL/OpenGLGraphicsManager.hpp"
#include "OpenGLApplication.hpp"

using namespace My;

namespace My {
// clang-format off
extern GfxConfiguration config;
IApplication*    g_pApp             = static_cast<IApplication*>(new OpenGLApplication(config));
GraphicsManager* g_pGraphicsManager = static_cast<GraphicsManager*>(new OpenGLGraphicsManager);
MemoryManager*   g_pMemoryManager   = static_cast<MemoryManager*>(new MemoryManager);
AssetLoader*     g_pAssetLoader     = static_cast<AssetLoader*>(new AssetLoader);
SceneManager*    g_pSceneManager    = static_cast<SceneManager*>(new SceneManager);
InputManager*    g_pInputManager    = static_cast<InputManager*>(new InputManager);
#ifdef DEBUG
DebugManager*    g_pDebugManager    = static_cast<DebugManager*>(new DebugManager);
#endif
// clang-format on
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
void OpenGLApplication::Tick() {
    g_pGraphicsManager->Clear();
    g_pGraphicsManager->Draw();
    GLFWApplication::Tick();
}
