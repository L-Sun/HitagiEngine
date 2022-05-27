#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/resource/scene.hpp>

namespace hitagi::resource {
class SceneManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    Scene& CurrentScene();

    Scene& CreateEmptyScene(std::string_view name = "");
    void   AddScene(Scene scene);
    void   SwitchScene(std::size_t index);
    void   DeleteScene(std::size_t index);
    void   DeleteScenes(std::pmr::vector<std::size_t> index_array);

    void SetCamera(std::shared_ptr<Camera> camera);

private:
    void CreateDefaultCamera(Scene& scene);
    void CreateDefaultLight(Scene& scene);

    std::pmr::vector<Scene> m_Scenes;
    std::size_t             m_CurrentScene;
    std::shared_ptr<Camera> m_CurrentCamera;
};
}  // namespace hitagi::resource

namespace hitagi {
extern std::unique_ptr<resource::SceneManager> g_SceneManager;
}