
#include <tchar.h>
#include "D3d12Application.hpp"
#include "D3d/D3d12GraphicsManager.hpp"

namespace My {
extern GfxConfiguration config;
IApplication* g_pApp = static_cast<IApplication*>(new D3d12Application(config));
GraphicsManager* g_pGraphicsManager =
    static_cast<GraphicsManager*>(new D3d12GraphicsManager);
MemoryManager* g_pMemoryManager =
    static_cast<MemoryManager*>(new MemoryManager);
AssetLoader*  g_pAssetLoader  = static_cast<AssetLoader*>(new AssetLoader);
SceneManager* g_pSceneManager = static_cast<SceneManager*>(new SceneManager);
InputManager* g_pInputManager = static_cast<InputManager*>(new InputManager);
#ifdef DEBUG
DebugManager* g_pDebugManager = static_cast<DebugManager*>(new DebugManager);
#endif
}  // namespace My