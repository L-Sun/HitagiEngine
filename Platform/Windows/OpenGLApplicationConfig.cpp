#include <tchar.h>
#include "OpenGLApplication.hpp"
#include "OpenGLGraphicsManager.hpp"

namespace My {
extern GfxConfiguration          config;
std::unique_ptr<IApplication>    g_pApp(new OpenGLApplication(config));
std::unique_ptr<GraphicsManager> g_pGraphicsManager(new OpenGLGraphicsManager);
std::unique_ptr<MemoryManager>   g_pMemoryManager(new MemoryManager);
std::unique_ptr<AssetLoader>     g_pAssetLoader(new AssetLoader);
std::unique_ptr<SceneManager>    g_pSceneManager(new SceneManager);
std::unique_ptr<InputManager>    g_pInputManager(new InputManager);
#ifdef DEBUG
std::unique_ptr<DebugManager> g_pDebugManager(new DebugManager);
#endif
}  // namespace My