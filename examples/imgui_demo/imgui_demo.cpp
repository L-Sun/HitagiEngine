#include "imgui_demo.hpp"

#include <hitagi/gfx/graphics_manager.hpp>
#include <hitagi/gui/gui_manager.hpp>
#include <hitagi/application.hpp>

using namespace hitagi;

void ImGuiDemo::Tick() {
    bool open = true;
    gui_manager->DrawGui([&]() -> void {
        ImGui::ShowDemoWindow(&open);
    });

    auto& render_graph = graphics_manager->GetRenderGraph();

    auto back_buffer = render_graph.ImportWithoutLifeTrack("BackBuffer", &graphics_manager->GetSwapChain().GetCurrentBackBuffer());

    struct ClearPass {
        gfx::ResourceHandle back_buffer;
    };
    auto clear_pass = render_graph.AddPass<ClearPass>(
        "ClearPass",
        [&](gfx::RenderGraph::Builder& builder, ClearPass& data) {
            data.back_buffer = builder.Write(back_buffer);
        },
        [=](const gfx::RenderGraph::ResourceHelper& helper, const ClearPass& data, gfx::GraphicsCommandContext* context) {
            context->SetRenderTarget(helper.Get<gfx::Texture>(data.back_buffer));
            context->ClearRenderTarget(helper.Get<gfx::Texture>(data.back_buffer));
        });

    auto output = gui_manager->GuiRenderPass(render_graph, clear_pass.back_buffer);
    render_graph.PresentPass(output);

    RuntimeModule::Tick();
}
