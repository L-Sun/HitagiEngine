#pragma once
#include <filesystem>

#include "IRuntimeModule.hpp"
#include "Scene.hpp"

namespace Hitagi::Asset {

class SceneManager : public IRuntimeModule {
public:
    int  Initialize() override;
    void Finalize() override;
    void Tick() override;

    void SetScene(std::shared_ptr<Scene> scene);

    bool IsSceneChanged();
    void NotifySceneIsRenderingQueued();
    void NotifySceneIsPhysicalSimulationQueued();

    const Scene& GetSceneForRendering() const;
    const Scene& GetSceneForPhysicsSimulation() const;

    void ResetScene();

    std::weak_ptr<SceneGeometryNode>   GetSceneGeometryNode(const std::string& name);
    std::weak_ptr<SceneLightNode>      GetSceneLightNode(const std::string& name);
    std::weak_ptr<SceneObjectGeometry> GetSceneGeometryObject(const std::string& key);

    std::weak_ptr<SceneCameraNode> GetCameraNode();

protected:
    std::shared_ptr<Scene> m_Scene;
    bool                   m_DirtyFlag = false;
};

}  // namespace Hitagi::Asset

namespace Hitagi {
extern std::unique_ptr<Asset::SceneManager> g_SceneManager;
}