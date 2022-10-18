#include "scene_viewport.hpp"
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/gfx/graphics_manager.hpp>

using namespace hitagi;

bool SceneViewPort::Initialize() {
    return RuntimeModule::Initialize();
}

void SceneViewPort::Tick() {
    struct SceneRenderPass {
        gfx::ResourceHandle depth_buffer;
        gfx::ResourceHandle output;
    };

    gui_manager->DrawGui([&]() {
        if (ImGui::Begin("Scene Viewer", &m_Open)) {
            if (m_CurrentScene) {
                auto window_size = ImGui::GetWindowSize();

                auto pass = graphics_manager->GetRenderGraph().AddPass<SceneRenderPass>(
                    "SceneViewPortRenderPass",
                    [&](gfx::RenderGraph::Builder& builder, SceneRenderPass& data) {
                        data.output = builder.Create(gfx::Texture::Desc{
                            .name   = "scene-output",
                            .width  = static_cast<std::uint32_t>(window_size.x),
                            .height = static_cast<std::uint32_t>(window_size.x),
                            .usages = gfx::Texture::UsageFlags::SRV | gfx::Texture::UsageFlags::RTV,
                        });

                        data.depth_buffer = builder.Create(gfx::Texture::Desc{
                            .name   = "scene-depth",
                            .width  = static_cast<std::uint32_t>(window_size.x),
                            .height = static_cast<std::uint32_t>(window_size.x),
                            .usages = gfx::Texture::UsageFlags::DSV,
                        });
                    },
                    [=](const gfx::RenderGraph::ResourceHelper& helper, const SceneRenderPass& data, gfx::GraphicsCommandContext* context) {
                        auto rtv = context->device.CreateTextureView({.textuer = helper.Get<gfx::Texture>(data.output)});
                        auto dsv = context->device.CreateTextureView({.textuer = helper.Get<gfx::Texture>(data.depth_buffer)});
                        // TODO draw scene
                    });

                ImGui::Image((void*)gui_manager->ReadTexture(pass.output).id, window_size);
            }
        }
        ImGui::End();
    });

    RuntimeModule::Tick();
}
