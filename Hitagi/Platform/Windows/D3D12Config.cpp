#include "D3D12GraphicsManager.hpp"
#include "GLFWApplication.hpp"
#include "MemoryManager.hpp"
#include "ResourceManager.hpp"
#include "SceneManager.hpp"
#include "InputManager.hpp"
#include "DebugManager.hpp"

namespace Hitagi {
extern GfxConfiguration                    config;
std::unique_ptr<IApplication>              g_App(new GLFWApplication(config));
std::unique_ptr<Graphics::GraphicsManager> g_GraphicsManager(new Graphics::DX12::D3D12GraphicsManager);
std::unique_ptr<Core::MemoryManager>       g_MemoryManager(new Core::MemoryManager);
std::unique_ptr<Core::FileIOManager>       g_FileIOManager(new Core::FileIOManager);
std::unique_ptr<Resource::ResourceManager> g_ResourceManager(new Resource::ResourceManager);
std::unique_ptr<Resource::SceneManager>    g_SceneManager(new Resource::SceneManager);
std::unique_ptr<InputManager>              g_InputManager(new InputManager);
std::unique_ptr<DebugManager>              g_DebugManager(new DebugManager);
}  // namespace Hitagi