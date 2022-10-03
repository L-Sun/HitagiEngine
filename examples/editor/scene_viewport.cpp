#include "scene_viewport.hpp"

#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

using namespace hitagi;
using namespace hitagi::resource;
using namespace hitagi::gfx;

bool SceneViewPort::Initialize() {
    m_Logger = spdlog::get("Editor");
    return true;
}

void SceneViewPort::Tick() {
    struct SceneViewPortPass {
        ResourceHandle depth_buffer;
        ResourceHandle scene_output;
    };

    gui_manager->DrawGui([&]() {
        if (ImGui::Begin("Scene Viewer", &m_Open)) {
            if (m_CurrentScene) {
                auto render_graph = graphics_manager->GetRenderGraph();
                auto curr_size    = ImGui::GetWindowSize();

                auto pass_data = render_graph->AddPass<SceneViewPortPass>(
                    "SceneViewPortPass",
                    [&](RenderGraph::Builder& builder, SceneViewPortPass& data) {
                        data.depth_buffer = builder.Create(
                            "SceneDepthBuffer",
                            Texture{
                                .bind_flags  = Texture::BindFlag::DepthBuffer,
                                .format      = Format::D32_FLOAT,
                                .width       = static_cast<std::uint32_t>(curr_size.x),
                                .height      = static_cast<std::uint32_t>(curr_size.y),
                                .clear_value = Texture::DepthStencil{
                                    .depth   = 1.0f,
                                    .stencil = 0,
                                },
                            });
                        data.scene_output = builder.Create(
                            "SceneOutput",
                            Texture{
                                .bind_flags  = Texture::BindFlag::RenderTarget | Texture::BindFlag::ShaderResource,
                                .format      = graphics_manager->sm_BackBufferFormat,
                                .width       = static_cast<std::uint32_t>(curr_size.x),
                                .height      = static_cast<std::uint32_t>(curr_size.y),
                                .clear_value = math::vec4f(0.2, 0.4, 0.2, 1.0),
                            });
                    },
                    [=](const RenderGraph::ResourceHelper& helper, const SceneViewPortPass& data, IGraphicsCommandContext* context) {
                        context->ClearRenderTarget(helper.Get<Texture>(data.scene_output));
                    });

                ImGui::Image((void*)pass_data.scene_output, curr_size);
            }
        }
        ImGui::End();
    });
}

void SceneViewPort::Finalize() {}

void SceneViewPort::SetScene(std::shared_ptr<resource::Scene> scene) {
    m_CurrentScene = std::move(scene);
}
