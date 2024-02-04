#include "scene_viewport.hpp"
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/asset/asset_manager.hpp>
#include <hitagi/asset/transform.hpp>
#include <hitagi/hid/input_manager.hpp>

using namespace hitagi;
using namespace hitagi::math;
using namespace hitagi::asset;

void SceneViewPort::Tick() {
    m_GuiManager.DrawGui([&]() {
        if (ImGui::Begin("Scene Viewer", &m_Open)) {
            if (m_CurrentScene) {
                const auto v_min       = ImGui::GetWindowContentRegionMin();
                const auto v_max       = ImGui::GetWindowContentRegionMax();
                const auto window_size = math::vec2u{
                    static_cast<std::uint32_t>(v_max.x - v_min.x),
                    static_cast<std::uint32_t>(v_max.y - v_min.y)};

                auto& render_graph = m_Render.GetRenderGraph();

                auto scene_render_texture = render_graph.Create(gfx::TextureDesc{
                    .name        = "Scene Render Texture",
                    .width       = window_size.x,
                    .height      = window_size.y,
                    .format      = gfx::Format::R8G8B8A8_UNORM,
                    .clear_value = math::Color{0.0f, 0.0f, 0.0f, 1.0f},
                    .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
                });

                auto camera           = m_Camera.GetComponent<asset::CameraComponent>().camera;
                auto camera_transform = m_Camera.GetComponent<asset::Transform>().world_matrix;

                camera->parameters.aspect = static_cast<float>(window_size.x) / static_cast<float>(window_size.y);

                if (window_size.x != 0 && window_size.y != 0) {
                    m_Render.RenderScene(m_CurrentScene, *camera, camera_transform, scene_render_texture);
                    ImGui::Image(m_GuiManager.ReadTexture(scene_render_texture), ImVec2(window_size.x, window_size.y));
                }
            }
        }
        ImGui::End();
    });

    RuntimeModule::Tick();
}

void SceneViewPort::SetScene(std::shared_ptr<asset::Scene> scene) noexcept {
    m_CurrentScene = std::move(scene);
    if (m_CurrentScene != nullptr) {
        m_Camera = m_CurrentScene->GetCurrentCamera();
        if (!m_Camera) {
            m_Camera = m_CurrentScene->CreateCameraEntity(std::make_shared<asset::Camera>(asset::Camera::Parameters{}), math::mat4f::identity(), {}, "view port camera");
        }
    }
}
