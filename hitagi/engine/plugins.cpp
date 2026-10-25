export module engine:plugins;
import std;
import core;
import app;
import ecs;
import platform;
import gfx;
import asset;
import physics;
import gui;
import debugger;
import hid;

export namespace hitagi {

struct InputPlugin {
    hid::InputState& input;
    void             Build(App& app) const { app.GetWorld().BorrowResource(input); }
};

struct PlatformContext {
    explicit PlatformContext(platform::PlatformConfig config) : host(platform::PlatformHost::Create(std::move(config))) {}
    std::unique_ptr<platform::PlatformHost> host;
};
struct PlatformPlugin {
    platform::PlatformConfig config;
    void                     Build(App& app) const {
        auto& owner = app.GetWorld().EmplaceResource<PlatformContext>(config);
        auto& host  = *owner.host;
        app.GetWorld().BorrowResource(host);
        app.AddPlugin(InputPlugin{.input = host.GetInputState()});
        app.SetEventLoop([&host] { host.PollEvents(); }, [&host] { return host.IsQuit(); });
        app.OnShutdown([&host] { host.SetModalFrameCallback({}); });
        host.SetModalFrameCallback([&app] { app.TickFrame(); });
    }
};

struct AssetPlugin {
    void Build(App& app) const {
        auto& assets = app.GetWorld().EmplaceResource<asset::AssetService>(files, jobs, root);
        app.OnShutdown([&assets] { assets.WaitForAsyncJobs(); });
    }

    core::IOService&      files;
    core::JobSystem&      jobs;
    std::filesystem::path root = "assets";
};
struct PhysicsPlugin {
    void Build(App& app) const { app.GetWorld().EmplaceResource<physics::PhysicsWorld>(jobs, config); }

    core::JobSystem&          jobs;
    physics::PhysicsWorldDesc config{};
};
struct DebugPlugin {
    struct System {
        void ConfigureSchedule(ecs::Schedule& schedule) { draw.ConfigureSchedule(schedule); }

        debugger::DebugDraw& draw;
    };
    void Build(App& app) const {
        auto& draw = app.GetWorld().EmplaceResource<debugger::DebugDraw>(FrameStage::First);
        app.GetWorld().RegisterSystem<System>(draw);
    }
};
struct GuiPlugin {
    platform::PlatformHost& platform;
    core::IOService&        files;
    void                    Build(App& app) const {
        app.GetWorld().EmplaceResource<gui::GuiContext>(platform, files);
        app.GetWorld().AddTask("Gui.Prepare", [](ecs::ResMut<gui::GuiContext> gui) { gui->PrepareFrame(); }, FrameStage::PreUpdate);
        app.GetWorld().AddTask("Gui.Compose", [](ecs::ResMut<gui::GuiContext> gui) { gui->Compose(); }, FrameStage::Render);
    }
};

struct DefaultPlugins {
    core::Runtime&           runtime;
    platform::PlatformConfig platform{};
    gfx::Device::Type        backend =
#ifdef __APPLE__
        gfx::Device::Type::Metal;
#else
        gfx::Device::Type::Vulkan;
#endif
    std::filesystem::path asset_root = "assets";
    void                  Build(App& app) const {
        auto config    = platform;
        config.surface = backend == gfx::Device::Type::Metal ? platform::Surface::Metal : platform::Surface::Vulkan;
        app.AddPlugins(TimePlugin{}, PlatformPlugin{.config = config}, gfx::GraphicsPlugin{.backend = backend},
                       AssetPlugin{.files = runtime.IO(), .jobs = runtime.Jobs(), .root = asset_root}, DebugPlugin{});
    }
};
}  // namespace hitagi
