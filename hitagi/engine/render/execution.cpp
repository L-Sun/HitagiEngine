export module render:execution;
import std;
import gfx;
import utils;
import app;
import math;

namespace hitagi::render::detail {

void ValidateOutput(const std::shared_ptr<gfx::Texture>& target) {
    if (!target) throw std::invalid_argument("Renderer submission requires a target texture");
    const auto& desc = target->GetDesc();
    if (!desc.width || !desc.height || desc.depth != 1 || desc.array_size != 1 ||
        !utils::has_flag(desc.usages, gfx::TextureUsageFlags::RenderTarget)) {
        throw std::invalid_argument("Renderer target must be a nonempty 2D render target");
    }
}

// Shared resource preparation only; ownership and frame execution remain in
// each concrete renderer. No runtime adapter or callback is involved.
bool PrepareWindowTarget(const Application& app, gfx::Device& device, gfx::CommandQueues& queues,
                         gfx::BindlessUtils& bindings, const std::shared_ptr<gfx::SwapChain>& swap_chain,
                         std::shared_ptr<gfx::Texture>& target) {
    // Window changes also include destruction, even when cached dimensions stay
    // unchanged. Let the swapchain reconcile that state before rendering.
    if (app.WindowSizeChanged() ||
        (!app.WindowsMinimized() &&
         (swap_chain->GetWidth() != app.GetWindowWidth() || swap_chain->GetHeight() != app.GetWindowHeight()))) {
        swap_chain->Resize();
    }
    if (app.WindowsMinimized() || app.GetWindowWidth() == 0 || app.GetWindowHeight() == 0) return false;

    if (target && (target->GetDesc().width != swap_chain->GetWidth() ||
                   target->GetDesc().height != swap_chain->GetHeight() ||
                   target->GetDesc().format != swap_chain->GetFormat())) {
        target.reset();
    }
    if (!target) {
        target = gfx::Texture::Create(
            device,
            queues,
            bindings,
            {
                .name        = "RendererWindowTarget",
                .width       = swap_chain->GetWidth(),
                .height      = swap_chain->GetHeight(),
                .format      = swap_chain->GetFormat(),
                .clear_value = math::Color::Black(),
                .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc,
            });
    }
    return true;
}

void FinishFrame(rg::RenderGraph& graph, rg::TextureHandle color, const std::shared_ptr<gfx::SwapChain>& swap_chain) {
    if (swap_chain) {
        rg::PresentPassBuilder present(graph);
        present.From(color);
        present.SetSwapChain(swap_chain);
        present.Finish();
    } else {
        // Keep the final write to the caller-owned texture alive without a
        // readback or copy queue transition. Imported resources alone are not roots.
        rg::RenderPassBuilder output(graph);
        output.SetName("RendererOutput");
        output.SetRenderTarget(graph.MoveFrom(color), false);
        output.AllowPassCulling(false);
        output.SetExecutor([](const rg::RenderGraph&, const rg::RenderPassNode&) {});
        output.Finish();
    }
    if (!graph.Compile()) throw std::runtime_error("Renderer graph compilation failed");
    graph.Execute();
    graph.ClearImportedResources();
    if (swap_chain) swap_chain->Present();
}

}  // namespace hitagi::render::detail
