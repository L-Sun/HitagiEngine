#pragma once
#include "geommath.hpp"
#include "IRuntimeModule.hpp"
#include "SceneParser.hpp"

namespace My {

class SceneManager : public IRuntimeModule {
public:
    virtual ~SceneManager();
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    int LoadScene(std::string_view);

    bool IsSceneChanged();
    void NotifySceneIsRenderingQueued();
    void NotifySceneIsPhysicalSimulationQueued();

    const Scene& GetSceneForRendering() const;
    const Scene& GetSceneForPhysicsSimulation() const;

    void ResetScene();

    std::weak_ptr<SceneGeometryNode> GetSceneGeometryNode(
        const std::string& name);
    std::weak_ptr<SceneObjectGeometry> GetSceneGeometryObject(
        const std::string& key);

protected:
    bool LoadOgexScene(std::string_view);

    std::shared_ptr<Scene> m_pScene;
    bool                   m_bDirtyFlag = false;
};

extern std::unique_ptr<SceneManager> g_pSceneManager;
}  // namespace My