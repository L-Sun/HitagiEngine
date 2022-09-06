#include "scene_viewport.hpp"

#include <hitagi/gui/gui_manager.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

using namespace hitagi;
using namespace hitagi::resource;
using namespace hitagi::graphics;

bool SceneViewPort::Initialize() {
    m_Logger = spdlog::get("Editor");

    m_RenderTexture         = std::make_shared<Texture>();
    m_RenderTexture->width  = 0;
    m_RenderTexture->height = 0;
    m_RenderTexture->format = Format::R8G8B8A8_UNORM;

    return true;
}

void SceneViewPort::Tick() {
    gui_manager->DrawGui([&]() {
        if (ImGui::Begin("Scene Viewer", &m_Open)) {
            auto curr_size = ImGui::GetWindowSize();
            if (curr_size.x != m_PrevSize.x || curr_size.y != m_PrevSize.y) {
                m_RenderTexture->width  = curr_size.x;
                m_RenderTexture->height = curr_size.y;
            }
            ImGui::Text("%s", fmt::format("Window width: {} px, height: {} px", ImGui::GetWindowWidth(), ImGui::GetWindowHeight()).c_str());
        }
        ImGui::End();
    });
}

void SceneViewPort::Finalize() {}

void SceneViewPort::SetScene(std::shared_ptr<resource::Scene> scene) {
    m_CurrentScene = std::move(scene);
}
