export module render:toon_plugin;
import std;
import core;
import app;
import ecs;
import asset;

import :plugin;
import :toon_renderer;

export namespace hitagi::render::toon {

struct ToonPlugin {
    asset::ResourceLoadContext resources;
    ToonSettings               settings{};
    void                       Build(App& app) const;
};

}  // namespace hitagi::render::toon

namespace hitagi::render::toon {

void ToonPlugin::Build(App& app) const {
    auto& world = app.GetWorld().GetResource<RenderApp>().app.GetWorld();
    world.EmplaceResource<ToonRenderer>(resources, settings);
}

}  // namespace hitagi::render::toon
