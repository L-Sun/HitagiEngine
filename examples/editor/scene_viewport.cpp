#include "scene_viewport.hpp"
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/asset/asset_manager.hpp>
#include <hitagi/asset/transform.hpp>
#include <hitagi/hid/input_manager.hpp>

using namespace hitagi;
using namespace hitagi::math;
using namespace hitagi::asset;

void SceneViewPort::Tick() {
    m_Engine.GuiManager().DrawGui([&]() {
        if (ImGui::Begin("Scene Viewer", &m_Open)) {
            MoveCamera();
            RenderScene();
        }
        ImGui::End();
    });

    RuntimeModule::Tick();
}

void SceneViewPort::MoveCamera() const {
    if (m_CurrentScene) {
        auto& input_manager = m_Engine.App().GetInputManager();

        auto camera = m_Camera;

        const auto& camera_param     = camera.Get<asset::CameraComponent>().camera->parameters;
        auto&       camera_transform = camera.Get<asset::Transform>();

        auto delta_time = m_Engine.GetDeltaTime().count();
        auto speed      = 10.0f * delta_time;

        if (input_manager.GetBool(hid::VirtualKeyCode::KEY_W)) {
            camera_transform.Translate(camera_param.look_dir * speed);
        }
        if (input_manager.GetBool(hid::VirtualKeyCode::KEY_S)) {
            camera_transform.Translate(-camera_param.look_dir * speed);
        }
        if (input_manager.GetBool(hid::VirtualKeyCode::KEY_A)) {
            camera_transform.Translate(-math::normalize(math::cross(camera_param.look_dir, camera_param.up)) * speed);
        }
        if (input_manager.GetBool(hid::VirtualKeyCode::KEY_D)) {
            camera_transform.Translate(math::normalize(math::cross(camera_param.look_dir, camera_param.up)) * speed);
        }

        if (input_manager.GetBool(hid::VirtualKeyCode::MOUSE_R_BUTTON)) {
            const auto mouse_delta = math::vec2f{input_manager.GetFloatDelta(hid::MouseEvent::MOVE_X), input_manager.GetFloatDelta(hid::MouseEvent::MOVE_Y)};
            camera_transform.Rotate(get_rotation(math::rotate_z(-mouse_delta.x * 0.01f) * math::rotate_x(-mouse_delta.y * 0.01f)));
        }
    }
}

void SceneViewPort::RenderScene() const {
    if (m_CurrentScene) {
        const auto v_min       = ImGui::GetWindowContentRegionMin();
        const auto v_max       = ImGui::GetWindowContentRegionMax();
        const auto window_size = math::vec2u{
            static_cast<std::uint32_t>(v_max.x - v_min.x),
            static_cast<std::uint32_t>(v_max.y - v_min.y)};

        auto& render       = m_Engine.Renderer();
        auto& render_graph = render.GetRenderGraph();

        auto scene_render_texture = render_graph.Create(gfx::TextureDesc{
            .name        = "Scene Render Texture",
            .width       = window_size.x,
            .height      = window_size.y,
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::Color{0.0f, 0.0f, 0.0f, 1.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::SRV,
        });

        auto camera           = m_Camera.Get<asset::CameraComponent>().camera;
        auto camera_transform = m_Camera.Get<asset::Transform>().world_matrix;

        camera->parameters.aspect = static_cast<float>(window_size.x) / static_cast<float>(window_size.y);

        if (window_size.x != 0 && window_size.y != 0) {
            render.RenderScene(m_CurrentScene, *camera, camera_transform, scene_render_texture);
            ImGui::Image(m_Engine.GuiManager().ReadTexture(scene_render_texture), ImVec2(window_size.x, window_size.y));
        }
    }
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
