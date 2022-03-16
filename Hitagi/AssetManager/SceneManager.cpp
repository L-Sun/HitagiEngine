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

    m_AnimationManager = std::make_unique<AnimationManager>();
    m_AnimationManager->Initialize();

    m_CurrentScene = std::make_shared<Scene>("Default Scene");
    m_Scenes.emplace(m_CurrentScene);

    CreateDefaultCamera(m_CurrentScene);
    CreateDefaultLight(m_CurrentScene);

    return result;
}
void SceneManager::Finalize() {
    m_CurrentScene = nullptr;

    for (auto&& scene : m_Scenes) {
        // here the variable `scene` is reference variable
        // use_count == 1 means that no one refers the scene except SceneManager
        if (scene.use_count() != 1) {
            m_Logger->warn("there is a Scene{} who is refered in other places!", scene->GetName());
        }
    }

    m_Scenes.clear();

    m_AnimationManager->Finalize();
    m_AnimationManager = nullptr;

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void SceneManager::Tick() {
    m_AnimationManager->Tick();
}

std::shared_ptr<Scene> SceneManager::CreateScene(std::string name) {
    auto result = std::make_shared<Scene>(std::move(name));
    m_Scenes.emplace(result);

    if (result->GetFirstCameraNode() == nullptr) CreateDefaultCamera(result);
    if (result->GetFirstLightNode() == nullptr) CreateDefaultLight(result);

    return result;
}

void SceneManager::AddScene(std::shared_ptr<Scene> scene) {
    assert(scene != nullptr);

    auto&& [iter, success] = m_Scenes.emplace(scene);

    if (scene->GetFirstCameraNode() == nullptr) CreateDefaultCamera(scene);
    if (scene->GetFirstLightNode() == nullptr) CreateDefaultLight(scene);
}

void SceneManager::SwitchScene(std::shared_ptr<Scene> scene) {
    if (m_Scenes.count(scene) == 0) {
        m_Logger->warn("you are try to switch a scene that is not managed by SceneManager!");
        m_Logger->warn("The scene will add to SceneManager!");
        m_Scenes.emplace(scene);
    }
    m_CurrentScene = scene;
}

void SceneManager::DeleteScene(std::shared_ptr<Scene> scene) {
    size_t delete_count = m_Scenes.erase(scene);
    if (delete_count == 0) {
        m_Logger->warn("You are trying to delete a scene that is not managed by SceneManager!");
    }
}

std::shared_ptr<Scene> SceneManager::GetSceneForRendering() const {
    auto result = std::make_shared<Scene>();
    for (auto&& node : m_CurrentScene->geometry_nodes) {
        if (node->Visible()) {
            result->geometry_nodes.emplace_back(node);
        }
    }
    result->camera_nodes = m_CurrentScene->camera_nodes;
    result->light_nodes  = m_CurrentScene->light_nodes;
    result->bone_nodes   = m_CurrentScene->bone_nodes;

    return result;
}

std::shared_ptr<Scene> SceneManager::GetSceneForPhysicsSimulation() const {
    return m_CurrentScene;
}

void SceneManager::CreateDefaultCamera(std::shared_ptr<Scene> scene) {
    const vec3f pos    = {5.0f, 5.0f, 5.0f};
    const vec3f up     = {0, 0, 1};
    const vec3f direct = -pos;

    auto camera      = std::make_shared<Camera>();
    auto camera_node = std::make_shared<CameraNode>("default-camera", pos, up, direct);
    camera->SetName("default-camera");
    camera_node->SetSceneObjectRef(camera);

    scene->cameras.emplace_back(camera);
    scene->camera_nodes.emplace_back(camera_node);
    scene->scene_graph->AppendChild(camera_node);
}

void SceneManager::CreateDefaultLight(std::shared_ptr<Scene> scene) {
    auto light       = std::make_shared<PointLight>();
    auto light_nodes = std::make_shared<LightNode>("default-light");
    light->SetName("default-light");
    light_nodes->SetSceneObjectRef(light);

    scene->lights.emplace_back(light);
    scene->light_nodes.emplace_back(light_nodes);
    scene->scene_graph->AppendChild(light_nodes);
}

}  // namespace Hitagi::Asset