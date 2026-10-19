#include "test_macros.hpp"
import interop.gtest;
import std;
import core;
import gfx;

using namespace hitagi;

namespace renderer_test {

class SceneUpdate final : public core::RuntimeModule {
public:
    explicit SceneUpdate(std::vector<int>& events) : RuntimeModule("Scene"), m_Events(events) {}
    ~SceneUpdate() final { m_Events.push_back(-1); }
    void Tick() final { ++frame; }

    int frame = 0;

private:
    std::vector<int>& m_Events;
};

class OffscreenRenderer final : public core::RuntimeModule {
public:
    OffscreenRenderer(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings,
                      const SceneUpdate& scene, std::vector<int>& events)
        : RuntimeModule("OffscreenRenderer"), m_Scene(scene), m_Events(events), m_Graph(device, queues, bindings) {}
    ~OffscreenRenderer() final { m_Events.push_back(-2); }

    void Tick() final {
        const auto            target = m_Graph.Create(gfx::TextureDesc{
            .name   = "Offscreen",
            .width  = 16,
            .height = 16,
            .format = gfx::Format::R8G8B8A8_UNORM,
            .usages = gfx::TextureUsageFlags::RenderTarget,
        });
        rg::RenderPassBuilder builder(m_Graph);
        builder.SetName("ApplicationPass");
        builder.SetRenderTarget(target, true);
        builder.AllowPassCulling(false);
        builder.SetExecutor([this, frame = m_Scene.frame](const rg::RenderGraph&, const rg::RenderPassNode&) {
            m_Events.push_back(frame);
        });
        builder.Finish();
        ASSERT_TRUE(m_Graph.Compile());
        m_Graph.Execute();
        m_Graph.ClearImportedResources();
    }

private:
    const SceneUpdate& m_Scene;
    std::vector<int>&  m_Events;
    rg::RenderGraph    m_Graph;
};

}  // namespace renderer_test

TEST(RendererModuleTest, ModuleTreeDrivesGraphAfterSceneAndDestroysConsumersFirst) {
    auto               device = gfx::create_device(gfx::Device::Type::Mock);
    gfx::CommandQueues queues(*device);
    auto               bindings = gfx::BindlessUtils::Create(*device);
    std::vector<int>   events;
    {
        core::RuntimeModule root("Root");
        auto                application = std::make_unique<core::RuntimeModule>("Application");
        auto                scene       = std::make_unique<renderer_test::SceneUpdate>(events);
        auto                renderer    = std::make_unique<renderer_test::OffscreenRenderer>(*device, queues, *bindings, *scene, events);
        application->AddSubModule(std::move(scene));
        application->AddSubModule(std::move(renderer));
        root.AddSubModule(std::move(application));
        root.Tick();
        root.Tick();
        root.Tick();
        EXPECT_EQ(events, (std::vector<int>{1, 2, 3}));
    }
    EXPECT_EQ(events, (std::vector<int>{1, 2, 3, -2, -1}));
}
