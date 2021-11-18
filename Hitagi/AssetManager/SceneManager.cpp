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
    m_Scene = nullptr;
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void SceneManager::Tick() {}

void SceneManager::SetScene(std::shared_ptr<Scene> scene) {
    m_Scene = scene;
    m_Scene->LoadResource();
    if (m_Scene->GetFirstCameraNode() == nullptr) {
        m_Logger->warn("Will create a default camera");
        m_Scene->cameras["default"] = std::make_shared<SceneObjectCamera>();

        vec3f pos                        = {3.0f, 3.0f, 3.0f};
        vec3f up                         = {-1, -1, 1};
        vec3f direct                     = -pos;
        m_Scene->camera_nodes["default"] = std::make_shared<SceneCameraNode>("default", pos, up, direct);
        m_Scene->camera_nodes["default"]->AddSceneObjectRef(m_Scene->cameras["default"]);
        m_Scene->scene_graph->AppendChild(m_Scene->camera_nodes["default"]);
    }
    if (m_Scene->GetFirstLightNode() == nullptr) {
        m_Logger->warn("Will create a default light.");
        m_Scene->lights["default"]      = std::make_shared<SceneObjectPointLight>();
        m_Scene->light_nodes["default"] = std::make_shared<SceneLightNode>("default");

        m_Scene->light_nodes["default"]->AddSceneObjectRef(m_Scene->lights["default"]);
        m_Scene->light_nodes["default"]->AppendTransform(
            std::make_shared<SceneObjectTranslation>(3.0f, 3.0f, 3.0f));

        m_Scene->scene_graph->AppendChild(m_Scene->light_nodes["default"]);
    }
    m_DirtyFlag = true;
}

void SceneManager::ResetScene() {
    m_Scene->scene_graph->Reset(true);
    m_Scene->LoadResource();
    m_DirtyFlag = true;
}

// TODO culling
const Scene& SceneManager::GetSceneForRendering() const {
    if (!m_Scene) m_Logger->error("Please attach a scene to SceneManager before get scene from SceneManager!");
    return *m_Scene;
}

const Scene& SceneManager::GetSceneForPhysicsSimulation() const {
    if (!m_Scene) m_Logger->error("Please attach a scene to SceneManager before get scene from SceneManager!");
    return *m_Scene;
}

bool SceneManager::IsSceneChanged() { return m_DirtyFlag; }

void SceneManager::NotifySceneIsRenderingQueued() { m_DirtyFlag = false; }

void SceneManager::NotifySceneIsPhysicalSimulationQueued() {}

std::weak_ptr<SceneGeometryNode> SceneManager::GetSceneGeometryNode(const std::string& name) {
    auto it = m_Scene->geometry_nodes.find(name);
    if (it != m_Scene->geometry_nodes.end())
        return it->second;
    else
        return {};
}
std::weak_ptr<SceneLightNode> SceneManager::GetSceneLightNode(const std::string& name) {
    auto it = m_Scene->light_nodes.find(name);
    if (it != m_Scene->light_nodes.end())
        return it->second;
    else
        return {};
}

std::weak_ptr<SceneObjectGeometry> SceneManager::GetSceneGeometryObject(const std::string& key) {
    return m_Scene->geometries.find(key)->second;
}
std::weak_ptr<SceneCameraNode> SceneManager::GetCameraNode() { return m_Scene->GetFirstCameraNode(); }

}  // namespace Hitagi::Asset