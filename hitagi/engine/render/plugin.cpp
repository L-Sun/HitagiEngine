export module render:plugin;
import std;
import app;
import ecs;
import gfx;
import platform;
import math;

import :execution;

export namespace hitagi::render {

// Main-world handle to the child App which owns rendering resources and tasks.
struct RenderApp {
    App& app;
};

struct RenderPlugin {
    gfx::Device&                  device;
    gfx::CommandQueues&           queues;
    gfx::BindlessUtils&           bindings;
    const platform::PlatformHost* window      = nullptr;
    math::Color                   clear_color = math::Color::Black();

    void Build(App& app) const;
};

}  // namespace hitagi::render

namespace hitagi::render {

void RenderPlugin::Build(App& app) const {
    auto& render_app = app.AddSubApp("Render");
    auto& world      = render_app.GetWorld();
    auto& context    = world.EmplaceResource<RenderContext>(device, queues, bindings, window, clear_color);
    render_app.OnShutdown([&context] { context.Shutdown(); });
    world.AddTask("Render.Execute", [](ecs::ResMut<RenderContext> context) { context->RenderPending(); }, FrameStage::Render);
    app.GetWorld().EmplaceResource<RenderApp>(RenderApp{.app = render_app});
}

}  // namespace hitagi::render
