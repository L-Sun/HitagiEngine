export module viewer:application;

import std;
import core;
import app;
import asset;
import ecs;
import math;
import hid;
import gfx;
import gui;
import render;

import :options;
import :scene;
import :camera;

export namespace hitagi::examples {

struct SceneNode {
    ecs::Entity entity;
    std::string path;
    std::string name;
    unsigned    depth  = 0;
    bool        hidden = false;
    math::AABBf bounds;
    math::AABBf                               local_bounds;
    std::shared_ptr<const asset::VertexArray> bounds_vertices;
    std::size_t parent_index = std::numeric_limits<std::size_t>::max();
};

struct UiControl {
    math::vec4f rect;
    bool        enabled = true;
};

class USDViewer final : public core::RuntimeModule {
public:
    USDViewer(Application& app, asset::ResourceLoadContext resources, core::FileIOManager& file_io,
              core::JobSystem& job_system, const std::filesystem::path& path, RendererType renderer, bool deterministic = false);
    ~USDViewer() final { UnloadAllSubModules(); }
    void Tick() final;
    void SetRenderer(RendererType renderer);

    // Layout and snapshots let the replay driver inspect actual UI/GPU results.
    auto Controls() const -> const std::map<std::string, UiControl>& { return m_Controls; }
    auto Snapshot() const -> std::string;
    auto Capture() -> asset::ImageData;
    auto FrameIndex() const noexcept { return m_Frame; }
    auto FrameTimings() const noexcept -> const std::array<double, 4>& { return m_FrameTimings; }

private:
    void OpenScene(const std::filesystem::path& path);
    void IndexScene(ecs::Entity entity, std::string parent, unsigned depth, std::size_t parent_index = std::numeric_limits<std::size_t>::max());
    void UpdateBounds();
    void ApplyCommand(std::string_view command);
    void DrainUi();
    void PublishState();
    void RenderFrame();
    auto SelectedNode() -> SceneNode*;
    auto Visible(const SceneNode& node) const -> bool;

    Application&                          m_App;
    asset::ResourceLoadContext            m_Resources;
    core::FileIOManager&                  m_FileIO;
    core::JobSystem&                      m_Jobs;
    std::shared_ptr<asset::Scene>         m_Scene;
    std::vector<SceneNode>                m_Nodes;
    std::filesystem::path                 m_Path;
    math::AABBf                           m_Bounds;
    asset::CameraComponent                m_Camera{};
    OrbitController                       m_Orbit;
    std::string                           m_Selected;
    std::string                           m_Error;
    std::string                           m_Status = "Ready";
    std::string                           m_PublishedState;
    std::string                           m_PublishedStaticState;
    std::chrono::steady_clock::time_point m_LastStatePublish{};
    float                                 m_Light = 1.0f;
    std::uint64_t                         m_Frame = 0;
    std::array<double, 4>                 m_FrameTimings{};
    std::chrono::steady_clock::time_point m_FpsSampleStart{};
    unsigned                             m_FpsSampleFrames = 0;
    double                               m_Fps = 0.0;
    std::vector<ecs::Entity>              m_AnimationPlayers;
    bool                                  m_Playing           = true;
    bool                                  m_Deterministic     = false;
    std::chrono::steady_clock::time_point m_LastAnimationTick = std::chrono::steady_clock::now();

    RendererType                           m_Renderer = RendererType::Pbr;
    render::deferred::RenderGraphDebugView m_Debug    = render::deferred::RenderGraphDebugView::Final;
    render::pbr::PbrRenderer*              m_Pbr      = nullptr;
    render::toon::ToonRenderer*            m_Toon     = nullptr;
    render::deferred::DeferredRenderer*    m_Deferred = nullptr;

    gui::GuiManager                  m_Gui;
    gui::GuiRenderUtils              m_GuiDraw;
    std::shared_ptr<gfx::SwapChain>  m_SwapChain;
    std::shared_ptr<gfx::Texture>    m_Output;
    math::vec4f                      m_Viewport{};
    std::map<std::string, UiControl> m_Controls;
    rg::RenderGraph                  m_Graph;
};

}  // namespace hitagi::examples

namespace hitagi::examples {

USDViewer::USDViewer(Application& app, asset::ResourceLoadContext resources, core::FileIOManager& file_io,
                     core::JobSystem& job_system, const std::filesystem::path& path, RendererType renderer, bool deterministic)
    : RuntimeModule("USDViewer"),
      m_App(app),
      m_Resources(resources),
      m_FileIO(file_io),
      m_Jobs(job_system),
      m_Deterministic(deterministic),
      m_Gui(app, file_io),
      m_GuiDraw(resources.device, resources.bindings, resources.shader_compiler),
      m_SwapChain(gfx::SwapChain::Create(resources.device, resources.queues.Get(gfx::CommandType::Graphics),
                                         {.name = "USDViewer", .window = app.GetWindow()})),
      m_Graph(resources.device, resources.queues, resources.bindings, "USDViewer") {
    m_Gui.LoadApp("build/pocketjs/ui/usd-viewer.js", "build/pocketjs/ui/usd-viewer.pak",
                  {.desktop = true, .raster_density = 2, .single_step = deterministic});
    SetRenderer(renderer);
    OpenScene(path);
    PublishState();
}

void USDViewer::SetRenderer(RendererType renderer) {
    if (renderer == m_Renderer && (m_Pbr || m_Toon || m_Deferred)) return;
    UnloadAllSubModules();
    m_Pbr      = nullptr;
    m_Toon     = nullptr;
    m_Deferred = nullptr;
    if (renderer == RendererType::Pbr) {
        auto module = std::make_unique<render::pbr::PbrRenderer>(m_Resources);
        m_Pbr       = module.get();
        AddSubModule(std::move(module));
    } else if (renderer == RendererType::Toon) {
        auto module = std::make_unique<render::toon::ToonRenderer>(m_Resources);
        m_Toon = module.get();
        AddSubModule(std::move(module));
    } else {
        auto module = std::make_unique<render::deferred::DeferredRenderer>(
            m_Resources.device, m_Resources.queues, m_Resources.bindings, m_Resources.shader_compiler,
            m_FileIO, "assets/shaders");
        m_Deferred = module.get();
        AddSubModule(std::move(module));
    }
    m_Renderer = renderer;
    m_Debug    = render::deferred::RenderGraphDebugView::Final;
    m_Status   = std::format("{} renderer", RendererName(renderer));
}

void USDViewer::Tick() {
    auto stamp = std::chrono::steady_clock::now();
    const auto measure = [&](unsigned index) {
        const auto end = std::chrono::steady_clock::now();
        m_FrameTimings[index] = std::chrono::duration<double, std::milli>(end - stamp).count();
        stamp = end;
    };
    ++m_Frame;
    m_Gui.PrepareFrame();
    DrainUi();
    measure(0);
    const auto   now     = std::chrono::steady_clock::now();
    const double elapsed = m_Deterministic ? 1.0 / 60.0 : std::chrono::duration<double>(now - m_LastAnimationTick).count();
    m_LastAnimationTick  = now;
    if (m_Scene && !m_AnimationPlayers.empty() && m_Playing) {
        m_Scene->GetWorld().Update(m_Jobs, elapsed);
        UpdateBounds();
    }
    measure(1);
    const auto& input = m_App.GetInputManager();
    if (!m_Gui.WantsTextInput()) {
        if (input.GetBoolNew(hid::VirtualKeyCode::KEY_F)) ApplyCommand("fit");
        if (input.GetBoolNew(hid::VirtualKeyCode::KEY_TAB))
            SetRenderer(m_Renderer == RendererType::Pbr ? RendererType::Toon :
                        m_Renderer == RendererType::Toon ? RendererType::Deferred : RendererType::Pbr);
        if (input.GetBoolNew(hid::VirtualKeyCode::KEY_ESCAPE)) m_Selected.clear();
    }
    if (m_Scene && m_Viewport.z > 0 && m_Viewport.w > 0) {
        const auto rect = m_Gui.DisplayRect(m_Viewport);
        m_Orbit.Update(m_Camera, input, rect.z / rect.w, rect.w,
                       !m_Gui.WantsTextInput() && m_Gui.PointerIn(m_Viewport));
    }
    RuntimeModule::Tick();
    RenderFrame();
    measure(2);
    // Average completed frame intervals, including event pumping and present waits.
    // Wall-clock metrics stay unavailable during deterministic replay.
    if (!m_Deterministic) {
        const auto completed = std::chrono::steady_clock::now();
        if (m_FpsSampleStart == std::chrono::steady_clock::time_point{}) {
            m_FpsSampleStart = completed;
        } else {
            ++m_FpsSampleFrames;
            const double seconds = std::chrono::duration<double>(completed - m_FpsSampleStart).count();
            if (seconds >= 0.5) {
                m_Fps = m_FpsSampleFrames / seconds;
                m_FpsSampleFrames = 0;
                m_FpsSampleStart = completed;
            }
        }
    }
    PublishState();
    measure(3);
}

}  // namespace hitagi::examples
