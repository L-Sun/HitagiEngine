#include "scene_viewport.hpp"

#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

using namespace hitagi;
using namespace hitagi::resource;
using namespace hitagi::graphics;

bool SceneViewPort::Initialize() {
    m_Logger = spdlog::get("Editor");

    std::size_t index = 0;
    for (auto& rt : m_RenderTextures) {
        rt             = std::make_shared<Texture>();
        rt->name       = fmt::format("SceneViewPort-{}", index++);
        rt->bind_flags = Texture::BindFlag::RenderTarget | Texture::BindFlag::ShaderResource;
        rt->format     = Format::R8G8B8A8_UNORM;
    }

    return true;
}

void SceneViewPort::Tick() {
    gui_manager->DrawGui([&]() {
        if (ImGui::Begin("Scene Viewer", &m_Open)) {
            auto rt        = m_RenderTextures.at(graphics_manager->GetCurrentBackBufferIndex());
            auto curr_size = ImGui::GetWindowSize();
            if (curr_size.x != rt->width || curr_size.y != rt->height) {
                rt->width  = curr_size.x;
                rt->height = curr_size.y;
                rt->pitch  = get_format_bit_size(rt->format) * curr_size.x;
                rt->dirty  = true;
            }
            if (m_CurrentScene) {
                graphics_manager->DrawScene(*m_CurrentScene);
                graphics_manager->DrawScene(*m_CurrentScene, rt);
                ImGui::Image(rt.get(), {static_cast<float>(rt->width), static_cast<float>(rt->height)});
            }
        }
        ImGui::End();
    });
}

void SceneViewPort::Finalize() {}

void SceneViewPort::SetScene(std::shared_ptr<resource::Scene> scene) {
    m_CurrentScene = std::move(scene);
}
