#include "test_macros.hpp"
import interop.gtest;
#ifdef __APPLE__
import interop.metal;
import interop.sdl;
#endif
import std;
import engine;

#ifdef __APPLE__
namespace graphics_frame_test {
using namespace hitagi;

struct FrameTask {
    Engine&                          engine;
    interop::mtl::TextureDescriptor& temporary;
    bool                             fail_prepare = true;
    bool                             fail_frame   = true;
    unsigned                         calls        = 0;

    void ConfigureSchedule(ecs::Schedule& schedule) {
        if (fail_prepare) throw std::runtime_error("prepare frame failure");
        schedule.Request("FrameTest.Last", [this] {
            ++calls;
            temporary.retain()->autorelease();
            // The frame guard must reject reentry and still drain the pool if
            // the final stage fails, after all rendering tasks could have run.
            engine.Tick();
            if (fail_frame) throw std::runtime_error("last stage failure"); }, FrameStage::Last);
    }
};
}  // namespace graphics_frame_test

TEST(GraphicsFrameTest, EngineEndsFrameAfterFailureAndCanRunNextFrame) {
    using namespace hitagi;
    namespace ns  = interop::ns;
    namespace mtl = interop::mtl;
    // Engine owns a MemoryManager; restore the test runner's default resource
    // after the engine and all of its PMR state have been destroyed.
    struct RestoreResource {
        std::pmr::memory_resource* previous = std::pmr::get_default_resource();
        ~RestoreResource() { std::pmr::set_default_resource(previous); }
    } restore;
    Engine     engine({.title = "GraphicsFrameTest", .gfx_backend = "Metal", .headless = true});
    auto       temporary = ns::TransferPtr(mtl::TextureDescriptor::alloc()->init());
    const auto count     = temporary->retainCount();
    auto&      task      = engine.GetWorld().RegisterSystem<graphics_frame_test::FrameTask>(engine, *temporary.get());

    EXPECT_THROW(engine.Tick(), std::runtime_error);
    EXPECT_EQ(task.calls, 0u);
    task.fail_prepare = false;

    // Keep a native pool outside Tick as in a Cocoa modal callback. Engine must
    // drain its own frame references without waiting for this pool to exit.
    auto native_pool = ns::TransferPtr(ns::AutoreleasePool::alloc()->init());
    EXPECT_THROW(engine.Tick(), std::runtime_error);
    EXPECT_EQ(task.calls, 1u);
    EXPECT_EQ(temporary->retainCount(), count);

    task.fail_frame = false;
    EXPECT_NO_THROW(engine.Tick());
    EXPECT_EQ(task.calls, 2u);
    EXPECT_EQ(temporary->retainCount(), count);
    EXPECT_NO_THROW(engine.Tick());
    EXPECT_EQ(task.calls, 3u);
    EXPECT_EQ(temporary->retainCount(), count);

    // SDL's live-resize watch calls Engine::TickFrame synchronously, bypassing
    // Engine::Tick. Cleanup must finish before returning to the native caller.
    SDL_Event event{};
    event.type            = SDL_EVENT_WINDOW_EXPOSED;
    event.window.windowID = SDL_GetWindowID(static_cast<SDL_Window*>(engine.App().GetWindow().ptr));
    event.window.data1    = 1;
    task.fail_frame       = true;
    EXPECT_TRUE(SDL_PushEvent(&event));
    EXPECT_EQ(task.calls, 4u);
    EXPECT_EQ(temporary->retainCount(), count);
    EXPECT_THROW(engine.App().PollEvents(), std::runtime_error);

    task.fail_frame = false;
    EXPECT_TRUE(SDL_PushEvent(&event));
    EXPECT_EQ(task.calls, 5u);
    EXPECT_EQ(temporary->retainCount(), count);
}
#endif
