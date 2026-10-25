#include "test_macros.hpp"
import std;
import interop.gtest;
import core;

namespace plugin_test {
struct Host {
    hitagi::core::PluginSet<Host> plugins;
    int                           value = 0;
    template <typename P>
    void AddPlugin(const P& plugin) { plugins.Add(*this, plugin); }
};
struct ValuePlugin {
    int& dependency;
    void Build(Host& host) const { host.value = dependency; }
};
struct CyclicPlugin {
    void Build(Host& host) const { host.AddPlugin(*this); }
};
struct FailingPlugin {
    void Build(Host& host) const {
        host.value = 7;
        throw std::runtime_error("install");
    }
};
}  // namespace plugin_test

TEST(PluginTest, InjectedDependencyAndIndependentHosts) {
    plugin_test::Host first, second;
    int               one = 1, two = 2;
    first.AddPlugin(plugin_test::ValuePlugin{one});
    second.AddPlugin(plugin_test::ValuePlugin{two});
    EXPECT_EQ(first.value, 1);
    EXPECT_EQ(second.value, 2);
    EXPECT_TRUE(first.plugins.Has<plugin_test::ValuePlugin>());
    EXPECT_THROW(first.AddPlugin(plugin_test::ValuePlugin{two}), std::logic_error);
    EXPECT_EQ(first.value, 1);
}
TEST(PluginTest, CyclesAndPartialFailurePoisonInstallation) {
    plugin_test::Host cycle, failed;
    EXPECT_THROW(cycle.AddPlugin(plugin_test::CyclicPlugin{}), std::logic_error);
    EXPECT_FALSE(cycle.plugins.IsValid());
    EXPECT_THROW(failed.AddPlugin(plugin_test::FailingPlugin{}), std::runtime_error);
    EXPECT_FALSE(failed.plugins.Has<plugin_test::FailingPlugin>());
    int value = 42;
    EXPECT_THROW(failed.AddPlugin(plugin_test::ValuePlugin{value}), std::logic_error);
}

TEST(PluginTest, CatchingRecursiveInstallDoesNotMakePartialPluginValid) {
    struct CatchCycle {
        void Build(plugin_test::Host& host) const {
            try {
                host.AddPlugin(*this);
            } catch (const std::logic_error&) {
            }
        }
    };
    plugin_test::Host host;
    EXPECT_THROW(host.AddPlugin(CatchCycle{}), std::logic_error);
    EXPECT_FALSE(host.plugins.IsValid());
    EXPECT_FALSE(host.plugins.Has<CatchCycle>());
}
