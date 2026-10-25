#include "test_macros.hpp"
import interop.gtest;
import std;
import core;
import ecs;
import engine;
import app;
import gfx;

using namespace hitagi;

namespace renderer_test {

class SceneUpdate final {
public:
    explicit SceneUpdate(std::vector<int>& events) : m_Events(events) {}
    ~SceneUpdate() { m_Events.emplace_back(-1); }
    void ConfigureSchedule(ecs::Schedule& schedule) {
        schedule.Request("Scene", [this] { ++frame; }, FrameStage::Update);
    }

    int frame = 0;

private:
    std::vector<int>& m_Events;
};

class OffscreenRenderer final {
public:
    OffscreenRenderer(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings,
                      const SceneUpdate& scene, std::vector<int>& events)
        : m_Scene(scene), m_Events(events), m_Graph(device, queues, bindings) {}
    ~OffscreenRenderer() { m_Events.emplace_back(-2); }

    void ConfigureSchedule(ecs::Schedule& schedule) {
        schedule.Request("Render", [this] { Render(); }, FrameStage::Render);
    }
    void Render() {
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
            m_Events.emplace_back(frame);
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

TEST(RendererSystemTest, ScheduleDrivesGraphAfterSceneAndDestroysConsumersFirst) {
    auto               device = gfx::create_device(gfx::Device::Type::Mock);
    gfx::CommandQueues queues(*device);
    auto               bindings = gfx::BindlessUtils::Create(*device);
    std::vector<int>   events;
    {
        core::JobSystem frame_jobs{core::JobSystem::Execution::CallingThread};
        ecs::World root("RenderWorld", ConfigureFrameStages);

        auto& scene = root.RegisterSystem<renderer_test::SceneUpdate>(events);
        root.RegisterSystem<renderer_test::OffscreenRenderer>(*device, queues, *bindings, scene, events);
        root.Update(frame_jobs);
        root.Update(frame_jobs);
        root.Update(frame_jobs);
        EXPECT_EQ(events, (std::vector<int>{1, 2, 3}));
    }
    EXPECT_EQ(events, (std::vector<int>{1, 2, 3, -2, -1}));
}
