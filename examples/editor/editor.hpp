#pragma once
#include "scene_viewport.hpp"
#include "imfilebrowser.hpp"

#include <hitagi/ecs/schedule.hpp>
#include <hitagi/engine.hpp>

#include <string>
#include <unordered_set>

namespace hitagi {
class Editor : public RuntimeModule {
public:
    bool Initialize() final;
    void Tick() final;

    inline std::string_view GetName() const noexcept final { return "Editor"; }

    void Render();

private:
    void MenuBar();
    void FileImporter();
    void SceneGraphViewer();
    void AssetExploer();

    core::Clock        m_Clock;
    SceneViewPort*     m_SceneViewPort = nullptr;
    ImGui::FileBrowser m_FileDialog;
};

}  // namespace hitagi