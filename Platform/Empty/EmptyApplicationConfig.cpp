#include "BaseApplication.hpp"

namespace My {
extern GfxConfiguration config;
IApplication* g_pApp = static_cast<IApplication*>(new BaseApplication(config));
GraphicsManager* g_pGraphicsManager =
    static_cast<GraphicsManager*>(new GraphicsManager);
MemoryManager* g_pMemoryManager =
    static_cast<MemoryManager*>(new MemoryManager);
AssetLoader*  g_pAssetLoader  = static_cast<AssetLoader*>(new AssetLoader);
SceneManager* g_pSceneManager = static_cast<SceneManager*>(new SceneManager);
InputManager* g_pInputManager = static_cast<InputManager*>(new InputManager);
}  // namespace My