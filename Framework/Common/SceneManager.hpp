#pragma once
#include <filesystem>
#include "IRuntimeModule.hpp"
#include "Scene.hpp"

namespace Hitagi {

class SceneManager : public IRuntimeModule {
public:
    virtual ~SceneManager();
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    int LoadScene(std::filesystem::path sceneFile);

    bool IsSceneChanged();
    void NotifySceneIsRenderingQueued();
    void NotifySceneIsPhysicalSimulationQueued();

    const Scene& GetSceneForRendering() const;
    const Scene& GetSceneForPhysicsSimulation() const;

    void ResetScene();

    std::weak_ptr<SceneGeometryNode>   GetSceneGeometryNode(const std::string& name);
    std::weak_ptr<SceneObjectGeometry> GetSceneGeometryObject(const std::string& key);

protected:
    std::unique_ptr<Scene> m_Scene;
    bool                   m_DirtyFlag = false;
    std::filesystem::path  m_ScenePath;
};

extern std::unique_ptr<SceneManager> g_SceneManager;
}  // namespace Hitagi