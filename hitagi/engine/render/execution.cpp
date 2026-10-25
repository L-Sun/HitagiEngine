export module render:execution;
import std;
import gfx;
import utils;
import platform;
import math;

export namespace hitagi::render {

// A builder borrows its renderer; captured frame data must own any retained arrays.
using FrameBuilder = std::function<rg::TextureHandle(rg::RenderGraph&, rg::TextureHandle)>;

class RenderContext {
public:
    RenderContext(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings,
                  const platform::PlatformHost* window = nullptr, math::Color clear_color = math::Color::Black());
    ~RenderContext();

    // Retained window composition, executed once per render tick. Empty clears it.
    void SetFrame(FrameBuilder build);
    // One offscreen composition; the target and builder are retained until execution.
    void Submit(FrameBuilder build, std::shared_ptr<gfx::Texture> target);
    void RenderPending();
    // Retire GPU work and captures before renderer resources are destroyed.
    void Shutdown();

    auto GetRenderedFrameCount() const noexcept -> std::uint64_t { return m_RenderedFrames; }
    auto GetWindowTarget() const noexcept -> const std::shared_ptr<gfx::Texture>& { return m_WindowTarget; }

private:
    void ExecuteFrame(const FrameBuilder& build, const std::shared_ptr<gfx::Texture>& target,
                      const std::shared_ptr<gfx::SwapChain>& swap_chain = {});

    struct Submission {
        FrameBuilder                  build;
        std::shared_ptr<gfx::Texture> target;
    };

    gfx::Device&                     m_Device;
    gfx::CommandQueues&              m_Queues;
    gfx::BindlessUtils&              m_Bindings;
    const platform::PlatformHost*    m_Window;
    math::Color                      m_ClearColor;
    std::shared_ptr<gfx::SwapChain>  m_SwapChain;
    std::shared_ptr<gfx::Texture>    m_WindowTarget;
    FrameBuilder                     m_WindowFrame;
    std::pmr::vector<Submission>     m_PendingFrames;
    std::uint64_t                    m_RenderedFrames = 0;
    bool                             m_Rendering      = false;
    bool                             m_Stopped        = false;
    std::unique_ptr<rg::RenderGraph> m_Graph;
};

}  // namespace hitagi::render

namespace hitagi::render::detail {

void ValidateOutput(const std::shared_ptr<gfx::Texture>& target) {
    if (!target) throw std::invalid_argument("Renderer submission requires a target texture");
    const auto& desc = target->GetDesc();
    if (!desc.width || !desc.height || desc.depth != 1 || desc.array_size != 1 ||
        !utils::has_flag(desc.usages, gfx::TextureUsageFlags::RenderTarget)) {
        throw std::invalid_argument("Renderer target must be a nonempty 2D render target");
    }
}

bool PrepareWindowTarget(
    const platform::PlatformHost&          app,
    gfx::Device&                           device,
    gfx::CommandQueues&                    queues,
    gfx::BindlessUtils&                    bindings,
    const std::shared_ptr<gfx::SwapChain>& swap_chain,
    std::shared_ptr<gfx::Texture>&         target,
    math::Color                            clear_color) {
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
                .clear_value = clear_color,
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

namespace hitagi::render {

RenderContext::RenderContext(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings,
                             const platform::PlatformHost* window, math::Color clear_color)
    : m_Device(device), m_Queues(queues), m_Bindings(bindings), m_Window(window), m_ClearColor(clear_color) {
    if (window) {
        m_SwapChain = gfx::SwapChain::Create(device, queues.Get(gfx::CommandType::Graphics),
                                             {.name = "RenderApp", .window = window->GetWindow()});
    }
}

RenderContext::~RenderContext() { Shutdown(); }

void RenderContext::SetFrame(FrameBuilder build) {
    if (m_Stopped) throw std::logic_error("RenderContext has stopped");
    if (!m_Window) throw std::logic_error("SetFrame requires a window RenderContext");
    if (m_Rendering) throw std::logic_error("Cannot replace a frame during rendering");
    m_WindowFrame = std::move(build);
}

void RenderContext::Submit(FrameBuilder build, std::shared_ptr<gfx::Texture> target) {
    if (m_Stopped) throw std::logic_error("RenderContext has stopped");
    if (!build) throw std::invalid_argument("Render submission requires a frame builder");
    detail::ValidateOutput(target);
    m_PendingFrames.push_back({.build = std::move(build), .target = std::move(target)});
}

void RenderContext::RenderPending() {
    if (m_Stopped) throw std::logic_error("RenderContext has stopped");
    if (m_Rendering) throw std::logic_error("Cannot render recursively");
    m_Rendering = true;
    struct Guard {
        bool& rendering;
        ~Guard() { rendering = false; }
    } guard{.rendering = m_Rendering};

    auto frames = std::exchange(m_PendingFrames, {});
    for (const auto& frame : frames) ExecuteFrame(frame.build, frame.target);
    if (m_WindowFrame && detail::PrepareWindowTarget(*m_Window, m_Device, m_Queues, m_Bindings,
                                                     m_SwapChain, m_WindowTarget, m_ClearColor)) {
        ExecuteFrame(m_WindowFrame, m_WindowTarget, m_SwapChain);
    }
}

void RenderContext::ExecuteFrame(const FrameBuilder& build, const std::shared_ptr<gfx::Texture>& target,
                                 const std::shared_ptr<gfx::SwapChain>& swap_chain) {
    try {
        if (!m_Graph) m_Graph = std::make_unique<rg::RenderGraph>(m_Device, m_Queues, m_Bindings, "RenderApp");
        const auto color = build(*m_Graph, m_Graph->Import(target));
        if (!m_Graph->IsValid(color)) throw std::logic_error("Frame builder returned an invalid output");
        detail::FinishFrame(*m_Graph, color, swap_chain);
        ++m_RenderedFrames;
    } catch (...) {
        m_Graph.reset();
        throw;
    }
}

void RenderContext::Shutdown() {
    if (m_Stopped) return;
    if (m_Rendering) throw std::logic_error("Cannot shut down during rendering");
    m_Stopped = true;
    m_Queues.WaitIdle();
    m_Graph.reset();
    m_PendingFrames.clear();
    m_WindowFrame = {};
    m_WindowTarget.reset();
    m_SwapChain.reset();
}

}  // namespace hitagi::render
