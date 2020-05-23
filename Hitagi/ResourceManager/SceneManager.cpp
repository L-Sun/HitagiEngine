#include "SceneManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "ResourceManager.hpp"

namespace Hitagi {
std::unique_ptr<Resource::SceneManager> g_SceneManager = std::make_unique<Resource::SceneManager>();
}

namespace Hitagi::Resource {

int SceneManager::Initialize() {
    int result = 0;
    m_Logger   = spdlog::stdout_color_mt("SceneManager");
    m_Logger->info("Initialize...");

    return result;
}
void SceneManager::Finalize() {
    m_Scene.clear();
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void SceneManager::Tick() {}

void SceneManager::SetScene(std::filesystem::path name) {
    m_Scene.emplace_back(g_ResourceManager->ParseScene(name));
    m_CurrentSceneIndex = m_Scene.size() - 1;
    if (m_Scene[m_CurrentSceneIndex].GetFirstCameraNode() == nullptr) {
        m_Logger->warn("Will create a default Camera");
        m_Scene[m_CurrentSceneIndex].Cameras["default"] = std::make_shared<SceneObjectCamera>();

        vec3f pos                                           = {3.0f, 3.0f, 3.0f};
        vec3f up                                            = {-1, -1, 1};
        vec3f direct                                        = -pos;
        m_Scene[m_CurrentSceneIndex].CameraNodes["default"] = std::make_shared<SceneCameraNode>("default", pos, up, direct);
        m_Scene[m_CurrentSceneIndex].CameraNodes["default"]->AddSceneObjectRef(m_Scene[m_CurrentSceneIndex].Cameras["default"]);
        m_Scene[m_CurrentSceneIndex].SceneGraph->AppendChild(m_Scene[m_CurrentSceneIndex].CameraNodes["default"]);
    }
    m_DirtyFlag = true;
}

void SceneManager::ResetScene() {
    m_Scene[m_CurrentSceneIndex].SceneGraph->Reset(true);
    m_DirtyFlag = true;
}

const Scene& SceneManager::GetSceneForRendering() const { return m_Scene[m_CurrentSceneIndex]; }

const Scene& SceneManager::GetSceneForPhysicsSimulation() const { return m_Scene[m_CurrentSceneIndex]; }

bool SceneManager::IsSceneChanged() { return m_DirtyFlag; }

void SceneManager::NotifySceneIsRenderingQueued() { m_DirtyFlag = false; }

void SceneManager::NotifySceneIsPhysicalSimulationQueued() {}

std::weak_ptr<SceneGeometryNode> SceneManager::GetSceneGeometryNode(const std::string& name) {
    auto it = m_Scene[m_CurrentSceneIndex].GeometryNodes.find(name);
    if (it != m_Scene[m_CurrentSceneIndex].GeometryNodes.end())
        return it->second;
    else
        return std::weak_ptr<SceneGeometryNode>();
}
std::weak_ptr<SceneLightNode> SceneManager::GetSceneLightNode(const std::string& name) {
    auto it = m_Scene[m_CurrentSceneIndex].LightNodes.find(name);
    if (it != m_Scene[m_CurrentSceneIndex].LightNodes.end())
        return it->second;
    else
        return std::weak_ptr<SceneLightNode>();
}

std::weak_ptr<SceneObjectGeometry> SceneManager::GetSceneGeometryObject(const std::string& key) {
    return m_Scene[m_CurrentSceneIndex].Geometries.find(key)->second;
}
std::weak_ptr<SceneCameraNode> SceneManager::GetCameraNode() { return m_Scene[m_CurrentSceneIndex].GetFirstCameraNode(); }

}  // namespace Hitagi::Resource