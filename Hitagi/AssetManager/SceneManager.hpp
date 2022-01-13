#pragma once
#include "IRuntimeModule.hpp"
#include "Scene.hpp"
#include "AnimationManager.hpp"

#include <set>

namespace Hitagi::Asset {

class SceneManager : public IRuntimeModule {
public:
    int  Initialize() override;
    void Finalize() override;
    void Tick() override;

    std::shared_ptr<Scene> CreateScene(std::string name);
    void                   AddScene(std::shared_ptr<Scene> scene);
    inline auto            ListAllScene() noexcept { return m_Scenes; }
    void                   SwitchScene(std::shared_ptr<Scene> scene);
    void                   DeleteScene(std::shared_ptr<Scene> scene);

    inline AnimationManager& GetAnimationManager() { return *m_AnimationManager; }

    inline auto GetScene() { return m_CurrentScene; }

    // TODO renderable culling
    std::shared_ptr<Scene> GetSceneForRendering() const;
    std::shared_ptr<Scene> GetSceneForPhysicsSimulation() const;

    std::weak_ptr<GeometryNode> GetSceneGeometryNode(const std::string& name);
    std::weak_ptr<LightNode>    GetSceneLightNode(const std::string& name);

    std::weak_ptr<CameraNode> GetCameraNode();

protected:
    static void CreateDefaultCamera(std::shared_ptr<Scene> scene);
    static void CreateDefaultLight(std::shared_ptr<Scene> scene);

    std::set<std::shared_ptr<Scene>> m_Scenes;
    std::shared_ptr<Scene>           m_CurrentScene;

    std::unique_ptr<AnimationManager> m_AnimationManager;
};

}  // namespace Hitagi::Asset

namespace Hitagi {
extern std::unique_ptr<Asset::SceneManager> g_SceneManager;
}