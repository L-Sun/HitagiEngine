#include <hitagi/resource/scene_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>

using namespace hitagi::math;

namespace hitagi {
resource::SceneManager* scene_manager = nullptr;
}

namespace hitagi::resource {

bool SceneManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("SceneManager");
    m_Logger->info("Initialize...");

    CreateEmptyScene("Default Scene");
    m_CurrentScene = 0;

    return true;
}
void SceneManager::Finalize() {
    m_Scenes.clear();

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void SceneManager::Tick() {
    for (auto& scene : m_Scenes)
        scene->root->Update();

    graphics_manager->DrawScene(CurrentScene());
}

Scene& SceneManager::CurrentScene() {
    return *m_Scenes.at(m_CurrentScene);
}

std::shared_ptr<Scene> SceneManager::CreateEmptyScene(std::string_view name) {
    auto& scene = m_Scenes.emplace_back(std::make_shared<Scene>(name));

    auto camera_node    = scene->camera_nodes.emplace_back(std::make_shared<CameraNode>("Default Camera"));
    camera_node->object = std::make_shared<Camera>();
    camera_node->Attach(scene->root);
    scene->curr_camera = camera_node;

    auto light_node    = scene->light_nodes.emplace_back(std::make_shared<LightNode>("Default Light"));
    light_node->object = std::make_shared<Light>();
    light_node->Attach(scene->root);

    return scene;
}

std::size_t SceneManager::AddScene(std::shared_ptr<Scene> scene) {
    if (scene) {
        m_Scenes.emplace_back(std::move(scene));
    }
    return m_Scenes.size() - 1;
}

std::size_t            SceneManager::GetNumScene() const noexcept { return m_Scenes.size(); }
std::shared_ptr<Scene> SceneManager::GetScene(std::size_t index) { return m_Scenes.at(index); }

void SceneManager::SwitchScene(std::size_t index) {
    if (index >= m_Scenes.size()) {
        return;
    }
    m_CurrentScene = index;
}

void SceneManager::DeleteScene(std::size_t index) {
    m_Scenes.erase(std::next(m_Scenes.begin(), index));
}

}  // namespace hitagi::resource