#include "scene_viewport.hpp"
#include <hitagi/gui/gui_manager.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

using namespace hitagi;
using namespace hitagi::resource;
using namespace hitagi::graphics;

bool SceneViewPort::Initialize() {
    m_Logger = spdlog::get("Editor");
    return true;
}

void SceneViewPort::Tick() {
    gui_manager->DrawGui([&]() {
        if (ImGui::Begin("Scene Viewer", &m_Open)) {
        }
        ImGui::End();
    });
}

void SceneViewPort::Finalize() {}

void SceneViewPort::SetScene(std::shared_ptr<resource::Scene> scene) {
    m_CurrentScene = scene;
}
