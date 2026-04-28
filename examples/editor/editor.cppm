module;

#include "imfilebrowser.hpp"
#include <spdlog/logger.h>

export module editor;
export import :scene_viewport;
export import :image_viewer;
import engine;

export namespace hitagi {
class Editor : public core::RuntimeModule {
public:
    Editor(Engine& engine);
    void Tick() final;

private:
    void MenuBar();
    void FileImporter();
    void SceneGraphViewer();
    void SceneNodeModifier();
    void AssetExplorer();

    Engine&      m_Engine;
    Application& m_App;

    core::Clock        m_Clock;
    SceneViewPort*     m_SceneViewPort = nullptr;
    ImageViewer*       m_ImageViewer   = nullptr;
    ImGui::FileBrowser m_FileDialog;

    ecs::Entity                   m_SelectedEntity;
    std::shared_ptr<asset::Scene> m_CurrScene = nullptr;
};

}  // namespace hitagi
