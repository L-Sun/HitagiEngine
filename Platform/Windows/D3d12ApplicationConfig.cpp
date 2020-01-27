
#include <tchar.h>
#include "D3D12Application.hpp"
#include "D3D12GraphicsManager.hpp"

namespace My {
extern GfxConfiguration          config;
std::unique_ptr<IApplication>    g_pApp(new D3D12Application(config));
std::unique_ptr<GraphicsManager> g_pGraphicsManager(new D3D12GraphicsManager);
std::unique_ptr<MemoryManager>   g_pMemoryManager(new MemoryManager);
std::unique_ptr<AssetLoader>     g_pAssetLoader(new AssetLoader);
std::unique_ptr<SceneManager>    g_pSceneManager(new SceneManager);
std::unique_ptr<InputManager>    g_pInputManager(new InputManager);
#ifdef DEBUG
std::unique_ptr<DebugManager> g_pDebugManager(new DebugManager);
#endif
}  // namespace My