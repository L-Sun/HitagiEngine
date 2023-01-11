#pragma once
#include "scene_viewport.hpp"
#include "imfilebrowser.hpp"

#include <hitagi/engine.hpp>

#include <string>
#include <unordered_set>

namespace hitagi {
class Editor : public RuntimeModule {
public:
    Editor(Engine& engine);
    void Tick() final;

private:
    void MenuBar();
    void FileImporter();
    void SceneGraphViewer();
    void SceneNodeModifier();
    void AssetExploer();

    Engine&      m_Engine;
    Application& m_App;

    core::Clock                       m_Clock;
    SceneViewPort*                    m_SceneViewPort = nullptr;
    ImGui::FileBrowser                m_FileDialog;
    std::shared_ptr<asset::SceneNode> m_SelectedNode = nullptr;
    std::shared_ptr<asset::Scene>     m_CurrScene    = nullptr;
};

}  // namespace hitagi