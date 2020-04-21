#include "SceneManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "ResourceManager.hpp"

namespace Hitagi::Resource {

SceneManager::~SceneManager() {}

int SceneManager::Initialize() {
    int result = 0;
    m_Logger   = spdlog::stdout_color_mt("SceneManager");
    m_Logger->info("Initialize...");

    return result;
}
void SceneManager::Finalize() {
    m_Scene = nullptr;
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void SceneManager::Tick() {}

void SceneManager::SetScene(std::filesystem::path name) {
    m_Scene     = g_ResourceManager->ParseScene(name);
    m_DirtyFlag = true;
}

void SceneManager::ResetScene() { m_DirtyFlag = true; }

const Scene& SceneManager::GetSceneForRendering() const { return *m_Scene; }

const Scene& SceneManager::GetSceneForPhysicsSimulation() const { return *m_Scene; }

bool SceneManager::IsSceneChanged() { return m_DirtyFlag; }

void SceneManager::NotifySceneIsRenderingQueued() { m_DirtyFlag = false; }

void SceneManager::NotifySceneIsPhysicalSimulationQueued() {}

std::weak_ptr<SceneGeometryNode> SceneManager::GetSceneGeometryNode(const std::string& name) {
    auto it = m_Scene->GeometryNodes.find(name);
    if (it != m_Scene->GeometryNodes.end())
        return it->second;
    else
        return std::weak_ptr<SceneGeometryNode>();
}
std::weak_ptr<SceneObjectGeometry> SceneManager::GetSceneGeometryObject(const std::string& key) {
    return m_Scene->Geometries.find(key)->second;
}
}  // namespace Hitagi::Resource