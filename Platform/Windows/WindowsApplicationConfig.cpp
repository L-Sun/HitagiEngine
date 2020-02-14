#include "D3D12GraphicsManager.hpp"
#include "WindowsApplication.hpp"

namespace My {
extern GfxConfiguration          config;
std::unique_ptr<IApplication>    g_pApp(new WindowsApplication(config));
std::unique_ptr<GraphicsManager> g_pGraphicsManager(new D3D12GraphicsManager);
std::unique_ptr<MemoryManager>   g_pMemoryManager(new MemoryManager);
std::unique_ptr<AssetLoader>     g_pAssetLoader(new AssetLoader);
std::unique_ptr<SceneManager>    g_pSceneManager(new SceneManager);
std::unique_ptr<InputManager>    g_pInputManager(new InputManager);
#ifdef DEBUG
std::unique_ptr<DebugManager> g_pDebugManager(new DebugManager);
#endif
}  // namespace My