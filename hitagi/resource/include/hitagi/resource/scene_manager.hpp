#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/resource/scene.hpp>

namespace hitagi::resource {
class SceneManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;
    void Finalize() final;

    Scene& CurrentScene();

    std::shared_ptr<Scene> CreateEmptyScene(std::string_view name = "");
    std::size_t            AddScene(std::shared_ptr<Scene> scene);
    std::size_t            GetNumScene() const noexcept;
    std::shared_ptr<Scene> GetScene(std::size_t index);
    void                   SwitchScene(std::size_t index);
    void                   DeleteScene(std::size_t index);

private:
    std::pmr::vector<std::shared_ptr<Scene>> m_Scenes;
    std::size_t                              m_CurrentScene;
};
}  // namespace hitagi::resource

namespace hitagi {
extern resource::SceneManager* scene_manager;
}