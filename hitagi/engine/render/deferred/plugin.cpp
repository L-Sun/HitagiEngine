export module render:deferred_plugin;
import std;
import core;
import app;
import ecs;
import asset;

import :plugin;
import :deferred_renderer;

export namespace hitagi::render::deferred {

struct DeferredPlugin {
    asset::ResourceLoadContext resources;
    core::IOService&           files;
    std::filesystem::path      shader_dir = "assets/shaders";
    void                       Build(App& app) const;
};

}  // namespace hitagi::render::deferred

namespace hitagi::render::deferred {

void DeferredPlugin::Build(App& app) const {
    auto& world = app.GetWorld().GetResource<RenderApp>().app.GetWorld();
    world.EmplaceResource<DeferredRenderer>(resources.device, resources.queues, resources.bindings, resources.shader_compiler, files, shader_dir);
}

}  // namespace hitagi::render::deferred
