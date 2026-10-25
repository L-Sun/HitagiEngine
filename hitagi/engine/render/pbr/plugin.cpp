export module render:pbr_plugin;
import std;
import core;
import app;
import ecs;
import asset;

import :plugin;
import :pbr_renderer;

export namespace hitagi::render::pbr {

struct PbrPlugin {
    asset::ResourceLoadContext resources;
    void                       Build(App& app) const;
};

}  // namespace hitagi::render::pbr

namespace hitagi::render::pbr {

void PbrPlugin::Build(App& app) const {
    auto& world = app.GetWorld().GetResource<RenderApp>().app.GetWorld();
    world.EmplaceResource<PbrRenderer>(resources);
}

}  // namespace hitagi::render::pbr
