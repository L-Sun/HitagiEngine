#pragma once
#include "IRuntimeModule.hpp"
#include "Scene.hpp"
#include "SceneParser.hpp"

#include <map>

namespace Hitagi::Asset {

class SceneManager : public IRuntimeModule {
public:
    int  Initialize() override;
    void Finalize() override;
    void Tick() override;

    Scene&       CreateScene(std::string name);
    Scene&       ImportScene(const std::filesystem::path& path);
    inline auto& ListAllScene() noexcept { return m_Scenes; }
    void         SwitchScene(xg::Guid id);
    void         DeleteScene(xg::Guid id);

    bool IsSceneChanged();
    void NotifySceneIsRenderingQueued();
    void NotifySceneIsPhysicalSimulationQueued();

    const Scene& GetSceneForRendering() const;
    const Scene& GetSceneForPhysicsSimulation() const;

    void ResetScene();

    std::weak_ptr<GeometryNode> GetSceneGeometryNode(const std::string& name);
    std::weak_ptr<LightNode>    GetSceneLightNode(const std::string& name);

    std::weak_ptr<CameraNode> GetCameraNode();

protected:
    static void CreateDefaultCamera(Scene& scene);
    static void CreateDefaultLight(Scene& scene);

    std::map<xg::Guid, Scene>    m_Scenes;
    xg::Guid                     m_CurrentScene;
    std::unique_ptr<SceneParser> m_Parser;
};

}  // namespace Hitagi::Asset

namespace Hitagi {
extern std::unique_ptr<Asset::SceneManager> g_SceneManager;
}