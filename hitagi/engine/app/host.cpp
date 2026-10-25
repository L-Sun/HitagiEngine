export module app:host;
import std;
import core;
import ecs;

import :frame_stages;

export namespace hitagi {

class App {
public:
    explicit App(core::Runtime& runtime, std::string_view name = "App");
    ~App();

    App(const App&)            = delete;
    App& operator=(const App&) = delete;

    auto GetWorld() noexcept -> ecs::World& { return *m_World; }
    auto GetDeltaTime() const noexcept { return m_Clock.DeltaTime(); }

    template <typename Plugin>
        requires core::PluginFor<Plugin, App>
    void AddPlugin(const Plugin& plugin) {
        if (m_InFrame || m_Stopped) throw std::logic_error("Cannot install plugin in active/stopped App");
        m_Plugins.Add(*this, plugin);
    }

    template <typename Plugin>
    bool HasPlugin() const { return m_Plugins.Has<Plugin>(); }

    template <typename... Plugins>
        requires(core::PluginFor<Plugins, App> && ...)
    void AddPlugins(const Plugins&... plugins) { (AddPlugin(plugins), ...); }

    // Configuration callbacks borrow resources which remain alive through Shutdown.
    void SetEventLoop(std::function<void()> poll, std::function<bool()> quit);
    void SetFrameCleanup(std::function<void()> cleanup);
    void OnShutdown(std::function<void()> shutdown) { m_Shutdown.emplace_back(std::move(shutdown)); }
    void RequestQuit() noexcept { m_Quit = true; }
    bool ShouldQuit() const { return m_Quit || m_Stopped || (m_ShouldQuit && m_ShouldQuit()); }

    int  Run(std::uint64_t max_frames = 0);
    void Tick();
    void TickFrame();

    auto AddSubApp(std::string_view name, std::function<void(ecs::World&, ecs::World&)> extract = {}) -> App&;

private:
    void Shutdown();

    struct SubApp {
        std::pmr::string                              name;
        std::unique_ptr<App>                          app;
        std::function<void(ecs::World&, ecs::World&)> extract;
    };

    core::Runtime&                          m_Runtime;
    core::PluginSet<App>                    m_Plugins;
    core::JobSystem                         m_FrameJobs{core::JobSystem::Execution::CallingThread};
    core::Clock                             m_Clock;
    std::unique_ptr<ecs::World>             m_World;
    std::pmr::vector<SubApp>                m_SubApps;
    std::pmr::vector<std::function<void()>> m_Shutdown;
    std::function<void()>                   m_Poll;
    std::function<void()>                   m_FrameCleanup;
    std::function<bool()>                   m_ShouldQuit;
    bool                                    m_InFrame = false;
    bool                                    m_Stopped = false;
    bool                                    m_Quit    = false;
};

struct Time {
    double        delta_seconds = 0.0;
    double        total_seconds = 0.0;
    std::uint64_t frame         = 0;
};

struct TimeSystem {
    static void ConfigureSchedule(ecs::Schedule& schedule);
};

struct TimePlugin {
    void Build(App& app) const;
};

}  // namespace hitagi

namespace hitagi {

App::App(core::Runtime& runtime, std::string_view name)
    : m_Runtime(runtime), m_World(std::make_unique<ecs::World>(name, ConfigureFrameStages)) {
    m_Clock.Start();
}

App::~App() { Shutdown(); }

void App::SetEventLoop(std::function<void()> poll, std::function<bool()> quit) {
    if (m_Poll) throw std::logic_error("App already has an event loop");
    m_Poll       = std::move(poll);
    m_ShouldQuit = std::move(quit);
}

void App::SetFrameCleanup(std::function<void()> cleanup) {
    if (m_FrameCleanup) throw std::logic_error("App already has frame cleanup");
    m_FrameCleanup = std::move(cleanup);
}

int App::Run(std::uint64_t max_frames) {
    if (!m_ShouldQuit && !max_frames) throw std::logic_error("Headless App::Run needs a frame limit or event loop");
    for (std::uint64_t frame = 0; !ShouldQuit() && (!max_frames || frame < max_frames); ++frame) Tick();
    return 0;
}

void App::Tick() {
    if (m_InFrame || m_Stopped) return;
    if (!m_Plugins.IsValid()) throw std::logic_error("App plugin installation failed");
    if (m_Poll) m_Poll();
    TickFrame();
}

void App::TickFrame() {
    if (m_InFrame || m_Stopped) return;
    if (!m_Plugins.IsValid()) throw std::logic_error("App plugin installation failed");
    m_InFrame = true;
    struct Guard {
        App& app;
        ~Guard() {
            if (app.m_FrameCleanup) app.m_FrameCleanup();
            app.m_InFrame = false;
        }
    } guard{.app = *this};
    m_World->Update(m_FrameJobs, m_Clock.DeltaTime().count());
    for (auto& sub : m_SubApps) {
        if (sub.extract) sub.extract(*m_World, sub.app->GetWorld());
        sub.app->TickFrame();
    }
    m_Clock.Tick();
}

auto App::AddSubApp(std::string_view name, std::function<void(ecs::World&, ecs::World&)> extract) -> App& {
    if (m_InFrame || m_Stopped) throw std::logic_error("Cannot add SubApp now");
    for (const auto& sub : m_SubApps)
        if (sub.name == name) throw std::logic_error("Duplicate SubApp");
    auto  app    = std::make_unique<App>(m_Runtime, name);
    auto& result = *app;
    m_SubApps.push_back({.name = std::pmr::string(name), .app = std::move(app), .extract = std::move(extract)});
    return result;
}

void App::Shutdown() {
    if (m_Stopped) return;
    m_Stopped = true;
    // Current services submit unscoped jobs; conservatively join the shared
    // pool while all consumers still exist. A per-App task scope can narrow this.
    m_Runtime.Jobs().WaitForAll();
    m_SubApps.clear();
    for (auto it = m_Shutdown.rbegin(); it != m_Shutdown.rend(); ++it) (*it)();
    m_Shutdown.clear();
    m_Poll         = {};
    m_ShouldQuit   = {};
    m_FrameCleanup = {};
    m_World.reset();
}

void TimeSystem::ConfigureSchedule(ecs::Schedule& schedule) {
    schedule.Request(
        "Time.Update",
        [&schedule](ecs::ResMut<Time> time) {
            time->delta_seconds = schedule.GetDeltaSeconds();
            time->total_seconds += time->delta_seconds;
            ++time->frame;
        },
        FrameStage::First);
}

void TimePlugin::Build(App& app) const {
    app.GetWorld().EmplaceResource<Time>();
    app.GetWorld().RegisterSystem<TimeSystem>();
}

}  // namespace hitagi
