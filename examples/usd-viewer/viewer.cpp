export module viewer:application;

import std;
import core;
import platform;
import asset;
import ecs;
import engine;
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
    ecs::Entity                               entity;
    std::string                               path;
    std::string                               name;
    unsigned                                  depth  = 0;
    bool                                      hidden = false;
    math::AABBf                               bounds;
    math::AABBf                               local_bounds;
    std::shared_ptr<const asset::VertexArray> bounds_vertices;
    std::size_t                               parent_index = std::numeric_limits<std::size_t>::max();
};

struct UiControl {
    math::vec4f rect;
    bool        enabled = true;
};

class USDViewer final {
public:
    USDViewer(platform::PlatformHost& app, asset::ResourceLoadContext resources, core::IOService& file_io,
              core::JobSystem& job_system, render::RenderContext& rendering,
              render::pbr::PbrRenderer& pbr, render::toon::ToonRenderer& toon, render::deferred::DeferredRenderer& deferred,
              const std::filesystem::path& path, RendererType renderer, bool deterministic = false);
    void ConfigureSchedule(ecs::Schedule& schedule);
    void SetRenderer(RendererType renderer);
    void FinishFrame();

    // Layout and snapshots let the replay driver inspect actual UI/GPU results.
    auto Controls() const -> const std::map<std::string, UiControl>& { return m_Controls; }
    auto Snapshot() const -> std::string;
    auto Capture() -> asset::ImageData;
    auto FrameIndex() const noexcept { return m_Frame; }
    auto FrameTimings() const noexcept -> const std::array<double, 4>& { return m_FrameTimings; }

private:
    void                                  PrepareFrame();
    void                                  UpdateScene();
    void                                  PublishFrame();
    void                                  Measure(unsigned phase);
    std::chrono::steady_clock::time_point m_PhaseStart;
    void                                  OpenScene(const std::filesystem::path& path);
    void                                  IndexScene(ecs::Entity entity, std::string parent, unsigned depth, std::size_t parent_index = std::numeric_limits<std::size_t>::max());
    void                                  UpdateBounds();
    void                                  ApplyCommand(std::string_view command);
    void                                  DrainUi();
    void                                  PublishState();
    void                                  RenderFrame();
    auto                                  BuildFrame(rg::RenderGraph& graph, rg::TextureHandle output) -> rg::TextureHandle;
    auto                                  SelectedNode() -> SceneNode*;
    auto                                  Visible(const SceneNode& node) const -> bool;

    platform::PlatformHost&               m_App;
    asset::ResourceLoadContext            m_Resources;
    core::IOService&                      m_FileIO;
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
    unsigned                              m_FpsSampleFrames = 0;
    double                                m_Fps             = 0.0;
    std::vector<ecs::Entity>              m_AnimationPlayers;
    bool                                  m_Playing           = true;
    bool                                  m_Deterministic     = false;
    std::chrono::steady_clock::time_point m_LastAnimationTick = std::chrono::steady_clock::now();

    RendererType                           m_Renderer = RendererType::Pbr;
    render::deferred::RenderGraphDebugView m_Debug    = render::deferred::RenderGraphDebugView::Final;
    render::RenderContext&                 m_Rendering;
    render::pbr::PbrRenderer&              m_Pbr;
    render::toon::ToonRenderer&            m_Toon;
    render::deferred::DeferredRenderer&    m_Deferred;

    gui::GuiContext                  m_Gui;
    gui::GuiRenderUtils              m_GuiDraw;
    math::vec4f                      m_Viewport{};
    std::map<std::string, UiControl> m_Controls;
};

// Installs the viewer without hiding its service dependencies.
struct USDViewerPlugin {
    platform::PlatformHost&    platform;
    asset::ResourceLoadContext resources;
    core::IOService&           files;
    core::JobSystem&           jobs;
    std::filesystem::path      scene;
    RendererType               renderer;
    bool                       deterministic = false;
    void                       Build(App& app) const {
        app.AddPlugin(render::RenderPlugin{
            .device      = resources.device,
            .queues      = resources.queues,
            .bindings    = resources.bindings,
            .window      = &platform,
            .clear_color = math::Color(0.067f, 0.082f, 0.11f, 1.0f),
        });
        app.AddPlugins(render::pbr::PbrPlugin{.resources = resources},
                       render::toon::ToonPlugin{.resources = resources},
                       render::deferred::DeferredPlugin{.resources = resources, .files = files});
        auto& render_world = app.GetWorld().GetResource<render::RenderApp>().app.GetWorld();
        auto& viewer       = app.GetWorld().RegisterSystem<USDViewer>(
            platform, resources, files, jobs, render_world.GetResource<render::RenderContext>(),
            render_world.GetResource<render::pbr::PbrRenderer>(), render_world.GetResource<render::toon::ToonRenderer>(),
            render_world.GetResource<render::deferred::DeferredRenderer>(), scene, renderer, deterministic);
        app.GetWorld().BorrowResource(viewer);
        render_world.AddTask("Viewer.Publish", [&viewer] { viewer.FinishFrame(); }, FrameStage::Last);
    }
};

}  // namespace hitagi::examples

namespace hitagi::examples {

USDViewer::USDViewer(platform::PlatformHost& app, asset::ResourceLoadContext resources, core::IOService& file_io,
                     core::JobSystem& job_system, render::RenderContext& rendering,
                     render::pbr::PbrRenderer& pbr, render::toon::ToonRenderer& toon, render::deferred::DeferredRenderer& deferred,
                     const std::filesystem::path& path, RendererType renderer, bool deterministic)
    : m_App(app),
      m_Resources(resources),
      m_FileIO(file_io),
      m_Jobs(job_system),
      m_Deterministic(deterministic),
      m_Rendering(rendering),
      m_Pbr(pbr),
      m_Toon(toon),
      m_Deferred(deferred),
      m_Gui(app, file_io),
      m_GuiDraw(resources.device, resources.bindings, resources.shader_compiler) {
    m_Gui.LoadApp("build/pocketjs/ui/usd-viewer.js", "build/pocketjs/ui/usd-viewer.pak",
                  {.desktop = true, .raster_density = 2, .single_step = deterministic});
    SetRenderer(renderer);
    OpenScene(path);
    PublishState();
}

void USDViewer::SetRenderer(RendererType renderer) {
    m_Renderer = renderer;
    m_Debug    = render::deferred::RenderGraphDebugView::Final;
    m_Status   = std::format("{} renderer", RendererName(renderer));
}

void USDViewer::ConfigureSchedule(ecs::Schedule& schedule) {
    schedule.Request("Viewer.Input", [this] { PrepareFrame(); }, FrameStage::PreUpdate);
    schedule.Request("Viewer.Scene", [this] { UpdateScene(); }, FrameStage::Update);
    schedule.Request("Viewer.Render", [this] { RenderFrame(); }, FrameStage::Render);
}

void USDViewer::FinishFrame() {
    Measure(2);
    PublishFrame();
}

void USDViewer::Measure(unsigned phase) {
    const auto end        = std::chrono::steady_clock::now();
    m_FrameTimings[phase] = std::chrono::duration<double, std::milli>(end - m_PhaseStart).count();
    m_PhaseStart          = end;
}

void USDViewer::PrepareFrame() {
    m_PhaseStart = std::chrono::steady_clock::now();
    ++m_Frame;
    m_Gui.PrepareFrame();
    DrainUi();
    Measure(0);
}

void USDViewer::UpdateScene() {
    const auto   now     = std::chrono::steady_clock::now();
    const double elapsed = m_Deterministic ? 1.0 / 60.0 : std::chrono::duration<double>(now - m_LastAnimationTick).count();
    m_LastAnimationTick  = now;
    if (m_Scene && !m_AnimationPlayers.empty() && m_Playing) {
        m_Scene->GetWorld().Update(m_Jobs, elapsed);
        UpdateBounds();
    }
    const auto& input = m_App.GetInputState();
    if (!m_Gui.WantsTextInput()) {
        if (input.GetBoolNew(hid::VirtualKeyCode::KEY_F)) ApplyCommand("fit");
        if (input.GetBoolNew(hid::VirtualKeyCode::KEY_TAB))
            SetRenderer(m_Renderer == RendererType::Pbr ? RendererType::Toon : m_Renderer == RendererType::Toon ? RendererType::Deferred
                                                                                                                : RendererType::Pbr);
        if (input.GetBoolNew(hid::VirtualKeyCode::KEY_ESCAPE)) m_Selected.clear();
    }
    if (m_Scene && m_Viewport.z > 0 && m_Viewport.w > 0) {
        const auto rect = m_Gui.DisplayRect(m_Viewport);
        m_Orbit.Update(m_Camera, input, rect.z / rect.w, rect.w,
                       !m_Gui.WantsTextInput() && m_Gui.PointerIn(m_Viewport));
    }
    Measure(1);
}

void USDViewer::PublishFrame() {
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
                m_Fps             = m_FpsSampleFrames / seconds;
                m_FpsSampleFrames = 0;
                m_FpsSampleStart  = completed;
            }
        }
    }
    PublishState();
    Measure(3);
}

}  // namespace hitagi::examples
