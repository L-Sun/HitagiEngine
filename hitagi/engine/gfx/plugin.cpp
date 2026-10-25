export module gfx:plugin;
import std;
import app;
import ecs;
import gfx.base;

import :utilities;
import :graphics_frame;

export namespace hitagi::gfx {

// Device outlives queues, bindings and all resources using their native addresses.
struct GraphicsContext {
    explicit GraphicsContext(Device::Type backend);
    ~GraphicsContext();

    std::unique_ptr<Device>        device;
    CommandQueues                  queues;
    std::unique_ptr<BindlessUtils> bindings;
    ShaderCompiler                 compiler;
};

struct GraphicsPlugin {
    Device::Type backend;
    void         Build(App& app) const;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {

GraphicsContext::GraphicsContext(Device::Type backend)
    : device(create_device(backend)), queues(*device), bindings(BindlessUtils::Create(*device)), compiler("App") {}

GraphicsContext::~GraphicsContext() { queues.WaitIdle(); }

void GraphicsPlugin::Build(App& app) const {
    auto& graphics = app.GetWorld().EmplaceResource<GraphicsContext>(backend);
    app.GetWorld().RegisterSystem<GraphicsFrameSystem>(*graphics.device, graphics.queues, FrameStage::First);
    app.SetFrameCleanup([&graphics] { graphics.device->EndFrame(); });
    app.OnShutdown([&graphics] { graphics.queues.WaitIdle(); });
}

}  // namespace hitagi::gfx
