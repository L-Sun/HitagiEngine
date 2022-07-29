#include <hitagi/resource/scene_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace hitagi::math;

namespace hitagi {
resource::SceneManager* scene_manager = nullptr;
}

namespace hitagi::resource {

bool SceneManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("SceneManager");
    m_Logger->info("Initialize...");

    m_Scenes.emplace_back(Scene{});
    m_CurrentScene = 0;

    CurrentScene().SetName("DefaultScene");

    CreateDefaultCamera(CurrentScene());
    CreateDefaultLight(CurrentScene());

    m_CurrentCamera = *(CurrentScene().cameras.begin());

    return true;
}
void SceneManager::Finalize() {
    m_Scenes.clear();

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void SceneManager::Tick() {
    graphics_manager->SetCamera(m_CurrentCamera);
    graphics_manager->AppendRenderables(CurrentScene().GetRenderable());
}

Scene& SceneManager::CurrentScene() {
    return m_Scenes[m_CurrentScene];
}

Scene& SceneManager::CreateEmptyScene(std::string_view name) {
    auto& scene = m_Scenes.emplace_back(Scene{});
    scene.SetName(name);

    CreateDefaultCamera(scene);
    CreateDefaultLight(scene);

    return scene;
}

std::size_t SceneManager::AddScene(Scene scene) {
    m_Scenes.emplace_back(scene);
    return m_Scenes.size();
}

std::size_t SceneManager::GetNumScene() const noexcept { return m_Scenes.size(); }
Scene&      SceneManager::GetScene(std::size_t index) { return m_Scenes.at(index); }

void SceneManager::SwitchScene(std::size_t index) {
    if (index >= m_Scenes.size()) {
        m_Logger->warn("You are setting a exsist scene!");
        return;
    }
    m_CurrentScene = index;
}

void SceneManager::DeleteScene(std::size_t index) {
    DeleteScenes({index});
}

void SceneManager::DeleteScenes(std::pmr::vector<std::size_t> index_array) {
    std::size_t i = 0, j = 0;
    std::sort(index_array.begin(), index_array.end());
    auto iter = std::remove_if(m_Scenes.begin(), m_Scenes.end(), [&](const Scene& scene) {
        if (i == index_array[j]) {
            i++;
            j++;
            return true;
        }
        i++;
        return false;
    });
    m_Scenes.erase(iter, m_Scenes.end());
}

void SceneManager::SetCamera(std::shared_ptr<Camera> camera) {
    m_CurrentCamera = std::move(camera);
}

void SceneManager::CreateDefaultCamera(Scene& scene) {
    auto camera = std::make_shared<Camera>();
    camera->SetName("default-camera");
    scene.cameras.emplace(camera);
}

void SceneManager::CreateDefaultLight(Scene& scene) {
    auto light = std::make_shared<PointLight>();
    light->SetName("default-light");

    scene.lights.emplace(light);
}

}  // namespace hitagi::resource