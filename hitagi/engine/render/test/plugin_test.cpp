#include "test_macros.hpp"
import std;
import app;
import core;
import ecs;
import gfx;
import render;

import interop.gtest;

using namespace hitagi;

TEST(RenderPluginTest, SeparateAppsOwnSeparateRenderingWorlds) {
    core::Runtime runtime({.worker_count = 1, .log_level = "off"});
    App           first(runtime);
    first.AddPlugin(gfx::GraphicsPlugin{.backend = gfx::Device::Type::Mock});
    auto& graphics = first.GetWorld().GetResource<gfx::GraphicsContext>();
    App   second(runtime);
    for (App* app : {&first, &second}) {
        app->AddPlugin(render::RenderPlugin{.device = *graphics.device, .queues = graphics.queues, .bindings = *graphics.bindings});
        app->AddPlugin(render::pbr::PbrPlugin{.resources = {
                                                  .device          = *graphics.device,
                                                  .queues          = graphics.queues,
                                                  .bindings        = *graphics.bindings,
                                                  .shader_compiler = graphics.compiler,
                                              }});
        EXPECT_FALSE(app->GetWorld().HasResource<render::pbr::PbrRenderer>());
        app->TickFrame();
    }
    auto& first_world  = first.GetWorld().GetResource<render::RenderApp>().app.GetWorld();
    auto& second_world = second.GetWorld().GetResource<render::RenderApp>().app.GetWorld();
    EXPECT_NE(&first_world, &second_world);
    EXPECT_NE(&first_world.GetResource<render::pbr::PbrRenderer>(), &second_world.GetResource<render::pbr::PbrRenderer>());
    EXPECT_EQ(first_world.GetResource<render::RenderContext>().GetRenderedFrameCount(), 0u);
    EXPECT_EQ(second_world.GetResource<render::RenderContext>().GetRenderedFrameCount(), 0u);
}

TEST(RenderPluginTest, ShutdownReleasesQueuedCapturesBeforeRenderingResources) {
    core::Runtime runtime({.worker_count = 1, .log_level = "off"});
    bool          provider_alive   = false;
    bool          capture_released = false;
    struct Provider {
        bool& alive;
        explicit Provider(bool& value) : alive(value) { alive = true; }
        ~Provider() { alive = false; }
    };
    {
        App app(runtime);
        app.AddPlugin(gfx::GraphicsPlugin{.backend = gfx::Device::Type::Mock});
        auto& graphics = app.GetWorld().GetResource<gfx::GraphicsContext>();
        app.AddPlugin(render::RenderPlugin{.device = *graphics.device, .queues = graphics.queues, .bindings = *graphics.bindings});
        auto& world = app.GetWorld().GetResource<render::RenderApp>().app.GetWorld();
        world.EmplaceResource<Provider>(provider_alive);
        auto capture = std::shared_ptr<int>(new int(0), [&](int* value) {
            EXPECT_TRUE(provider_alive);
            capture_released = true;
            delete value;
        });
        auto target  = gfx::Texture::Create(*graphics.device, graphics.queues, *graphics.bindings, {
                                                                                                       .width  = 2,
                                                                                                       .height = 2,
                                                                                                       .format = gfx::Format::R8G8B8A8_UNORM,
                                                                                                       .usages = gfx::TextureUsageFlags::RenderTarget,
                                                                                                   });
        world.GetResource<render::RenderContext>().Submit(
            [capture](rg::RenderGraph&, rg::TextureHandle target) { return target; }, target);
        capture.reset();
        EXPECT_FALSE(capture_released);
        // No frame runs: shutdown must still retire the pending callback first.
    }
    EXPECT_TRUE(capture_released);
    EXPECT_FALSE(provider_alive);
}
