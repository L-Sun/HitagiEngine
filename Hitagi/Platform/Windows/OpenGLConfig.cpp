#include "OpenGLGraphicsManager.hpp"
#include "GLFWApplication.hpp"
#include "MemoryManager.hpp"
#include "ResourceManager.hpp"
#include "SceneManager.hpp"
#include "InputManager.hpp"
#include "DebugManager.hpp"
#include "ThreadManager.hpp"

namespace Hitagi {
extern GfxConfiguration                    config;
std::unique_ptr<IApplication>              g_App(new GLFWApplication(config));
std::unique_ptr<Graphics::GraphicsManager> g_GraphicsManager(new Graphics::OpenGL::OpenGLGraphicsManager);
}  // namespace Hitagi