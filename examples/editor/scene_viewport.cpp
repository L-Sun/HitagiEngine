#include "scene_viewport.hpp"
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/asset/asset_manager.hpp>
#include <hitagi/hid/input_manager.hpp>

using namespace hitagi;
using namespace hitagi::math;
using namespace hitagi::asset;

void SceneViewPort::Tick() {
    struct SceneRenderPass {
        gfx::ResourceHandle depth_buffer;
        gfx::ResourceHandle output;
    };

    m_GuiManager.DrawGui([&]() {
        if (ImGui::Begin("Scene Viewer", &m_Open)) {
            if (m_CurrentScene) {
                const auto v_min       = ImGui::GetWindowContentRegionMin();
                const auto v_max       = ImGui::GetWindowContentRegionMax();
                const auto window_size = math::vec2u{
                    static_cast<std::uint32_t>(v_max.x - v_min.x),
                    static_cast<std::uint32_t>(v_max.y - v_min.y)};

                m_Camera->GetObjectRef()->parameters.aspect = static_cast<float>(window_size.x) / static_cast<float>(window_size.y);
                m_Camera->Update();

                if (window_size.x != 0 && window_size.y != 0) {
                    const auto output = m_Render.RenderScene(*m_CurrentScene, *m_Camera, std::nullopt, window_size);
                    ImGui::Image((void*)m_GuiManager.ReadTexture(output).id, ImVec2(window_size.x, window_size.y));
                }
            }
        }
        ImGui::End();
    });

    RuntimeModule::Tick();
}

void SceneViewPort::SetScene(std::shared_ptr<asset::Scene> scene) noexcept {
    m_CurrentScene = std::move(scene);
    if (m_Camera == nullptr && m_CurrentScene != nullptr) {
        auto camera = std::make_shared<Camera>(m_CurrentScene->curr_camera->GetObjectRef()->parameters);
        m_Camera    = std::make_shared<CameraNode>(camera, m_CurrentScene->curr_camera->transform);
    }
}
