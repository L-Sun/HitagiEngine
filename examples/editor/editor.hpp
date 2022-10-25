#pragma once
#include "scene_viewport.hpp"
#include "imfilebrowser.hpp"

#include <hitagi/core/runtime_module.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

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
    std::pmr::string                          m_OpenFileExt;
    std::pmr::unordered_set<std::pmr::string> m_SelectedFiles;
    SceneViewPort*                            m_SceneViewPort = nullptr;
    ImGui::FileBrowser                        m_FileDialog;
};

}  // namespace hitagi