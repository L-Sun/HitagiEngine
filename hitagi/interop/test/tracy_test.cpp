#include "test_macros.hpp"
#include "interop/tracy_macros.hpp"

import std;
import interop.gtest;
import interop.tracy;

#ifdef TRACY_ENABLE
static_assert(hitagi::interop::profiling_enabled);
#else
static_assert(!hitagi::interop::profiling_enabled);
#endif

TEST(InteropTracyTest, ScopeAndLockPreserveCallerLifetime) {
    ZoneScopedN("Interop caller scope");
    TracyLockableN(std::mutex, mutex, "Interop test mutex");
    {
        std::scoped_lock lock(mutex);
        ZoneScopedN("Interop nested scope");
        ZoneText("nested", 6);
    }
    ZoneName("outer scope is still alive", 26);
    ASSERT_TRUE(mutex.try_lock());
    mutex.unlock();
}

#ifndef TRACY_ENABLE
TEST(InteropTracyTest, DisabledInstrumentationDoesNotEvaluateArguments) {
    int evaluations = 0;
    ZoneScopedN(++evaluations);
    ZoneScopedNS(++evaluations, ++evaluations);
    ZoneName(++evaluations, ++evaluations);
    ZoneText(++evaluations, ++evaluations);
    TracyPlot(++evaluations, ++evaluations);
    TracyPlotConfig(++evaluations, ++evaluations, ++evaluations, ++evaluations, ++evaluations);
    TracyAllocN(++evaluations, ++evaluations, ++evaluations);
    TracyFreeN(++evaluations, ++evaluations);
    TracyMessageCS(++evaluations, ++evaluations, ++evaluations, ++evaluations);
    TracyMessageLCS(++evaluations, ++evaluations, ++evaluations);
    EXPECT_EQ(TracyVkContext(++evaluations, ++evaluations, ++evaluations, ++evaluations), nullptr);
    TracyVkDestroy(++evaluations);
    TracyVkContextName(++evaluations, ++evaluations, ++evaluations);
    TracyVkCollect(++evaluations, ++evaluations);
    EXPECT_EQ(TracyD3D12Context(++evaluations, ++evaluations), nullptr);
    TracyD3D12Destroy(++evaluations);
    TracyD3D12ContextName(++evaluations, ++evaluations, ++evaluations);
    TracyD3D12NewFrame(++evaluations);
    TracyD3D12Collect(++evaluations);
    EXPECT_EQ(evaluations, 0);
}
#endif
