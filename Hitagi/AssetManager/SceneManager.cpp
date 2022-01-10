#include "SceneManager.hpp"

#include "FileIOManager.hpp"
#include "Assimp.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Hitagi {
std::unique_ptr<Asset::SceneManager> g_SceneManager = std::make_unique<Asset::SceneManager>();
}

namespace Hitagi::Asset {

int SceneManager::Initialize() {
    int result = 0;
    m_Logger   = spdlog::stdout_color_mt("SceneManager");
    m_Logger->info("Initialize...");
    m_Parser               = std::make_unique<AssimpParser>();
    auto&& [iter, success] = m_Scenes.emplace(xg::newGuid(), Scene{"Default"});
    m_CurrentScene         = iter->first;
    Scene& scene           = iter->second;

    CreateDefaultCamera(scene);
    CreateDefaultLight(scene);

    return result;
}
void SceneManager::Finalize() {
    m_Logger->info("Finalized.");
    m_Scenes.clear();
    m_Parser = nullptr;
    m_Logger = nullptr;
}

void SceneManager::Tick() {}

Scene& SceneManager::CreateScene(std::string name) {
    auto&& [iter, success] = m_Scenes.emplace(xg::newGuid(), Scene{std::move(name)});
    m_CurrentScene         = iter->first;
    Scene& scene           = iter->second;

    if (scene.GetFirstCameraNode() == nullptr) CreateDefaultCamera(scene);
    if (scene.GetFirstLightNode() == nullptr) CreateDefaultLight(scene);

    return scene;
}

Scene& SceneManager::ImportScene(const std::filesystem::path& path) {
    auto&& [iter, success] = m_Scenes.emplace(xg::newGuid(), m_Parser->Parse(g_FileIoManager->SyncOpenAndReadBinary(path)));

    m_CurrentScene = iter->first;
    Scene& scene   = iter->second;
    scene.SetName(path.stem().string());

    if (scene.GetFirstCameraNode() == nullptr) CreateDefaultCamera(scene);
    if (scene.GetFirstLightNode() == nullptr) CreateDefaultLight(scene);

    return scene;
}

void SceneManager::SwitchScene(xg::Guid id) {
    if (m_Scenes.count(id) != 0) {
        m_CurrentScene = id;
    }
}

void SceneManager::DeleteScene(xg::Guid id) {
    size_t delete_count = m_Scenes.erase(id);
    if (delete_count == 0) {
        m_Logger->warn("You are trying to delete a non-exsist scene!");
    }
}

// TODO culling
const Scene& SceneManager::GetSceneForRendering() const {
    return m_Scenes.at(m_CurrentScene);
}

const Scene& SceneManager::GetSceneForPhysicsSimulation() const {
    return m_Scenes.at(m_CurrentScene);
}

void SceneManager::CreateDefaultCamera(Scene& scene) {
    scene.cameras["default"] = std::make_shared<Camera>();

    vec3f pos                     = {3.0f, 3.0f, 3.0f};
    vec3f up                      = {-1, -1, 1};
    vec3f direct                  = -pos;
    scene.camera_nodes["default"] = std::make_shared<CameraNode>("default", pos, up, direct);
    scene.camera_nodes["default"]->SetSceneObjectRef(scene.cameras["default"]);
    scene.scene_graph->AppendChild(scene.camera_nodes["default"]);
}

void SceneManager::CreateDefaultLight(Scene& scene) {
    scene.lights["default"]      = std::make_shared<PointLight>();
    scene.light_nodes["default"] = std::make_shared<LightNode>("default");

    scene.light_nodes["default"]->SetSceneObjectRef(scene.lights["default"]);
    scene.light_nodes["default"]->ApplyTransform(translate(mat4f(1.0f), vec3f(3.0f, 3.0f, 3.0f)));

    scene.scene_graph->AppendChild(scene.light_nodes["default"]);
}

}  // namespace Hitagi::Asset