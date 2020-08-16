#include "SceneManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "AssetManager.hpp"

namespace Hitagi {
std::unique_ptr<Asset::SceneManager> g_SceneManager = std::make_unique<Asset::SceneManager>();
}

namespace Hitagi::Asset {

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
    m_Scene.emplace_back(g_AssetManager->ParseScene(name));
    m_CurrentSceneIndex = m_Scene.size() - 1;
    auto& scene         = m_Scene[m_CurrentSceneIndex];
    if (scene.GetFirstCameraNode() == nullptr) {
        m_Logger->warn("Will create a default camera");
        scene.Cameras["default"] = std::make_shared<SceneObjectCamera>();

        vec3f pos                    = {3.0f, 3.0f, 3.0f};
        vec3f up                     = {-1, -1, 1};
        vec3f direct                 = -pos;
        scene.CameraNodes["default"] = std::make_shared<SceneCameraNode>("default", pos, up, direct);
        scene.CameraNodes["default"]->AddSceneObjectRef(scene.Cameras["default"]);
        scene.SceneGraph->AppendChild(scene.CameraNodes["default"]);
    }
    if (scene.GetFirstLightNode() == nullptr) {
        m_Logger->warn("Will create a default light.");
        scene.Lights["default"]     = std::make_shared<SceneObjectPointLight>();
        scene.LightNodes["default"] = std::make_shared<SceneLightNode>("default");

        scene.LightNodes["default"]->AddSceneObjectRef(scene.Lights["default"]);
        scene.LightNodes["default"]->AppendTransform(
            std::make_shared<SceneObjectTranslation>(3.0f, 3.0f, 3.0f));

        scene.SceneGraph->AppendChild(scene.LightNodes["default"]);
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

}  // namespace Hitagi::Asset