#include "test_macros.hpp"
import interop.gtest;
import std;
import core;
import ecs;

namespace resource_test {
struct Counter {
    int value = 0;
};
struct Position {
    int value = 0;
};
enum class Phase { Write,
                   Read };
void Configure(hitagi::ecs::Schedule& schedule) { schedule.SetOrder(Phase::Write, Phase::Read); }
}  // namespace resource_test
using namespace hitagi;
using resource_test::Counter;
using resource_test::Phase;
using resource_test::Position;

TEST(ResourceTest, OrderedResourcesAndQueryRunOnCachedSchedule) {
    ecs::World      world("ResourceQuery", resource_test::Configure);
    core::JobSystem jobs(2);
    world.EmplaceResource<Counter>();
    auto entity = world.GetEntityManager().Create(Position{});
    world.AddTask("write", [](ecs::ResMut<Counter> counter) { ++counter->value; }, Phase::Write);
    world.AddTask("query", [](ecs::Query<Position&> query, ecs::Res<Counter> counter) { query.Each([&](Position& position) { position.value += counter->value; }); }, Phase::Read);
    world.Update(jobs);
    world.Update(jobs);
    EXPECT_EQ(entity.Get<Position>().value, 3);
    EXPECT_EQ(world.GetResource<Counter>().value, 2);
}
TEST(ResourceTest, ConflictsAndMissingResourcesFailBeforeExecution) {
    ecs::World      world("Conflict");
    core::JobSystem jobs(2);
    world.EmplaceResource<Counter>();
    world.AddTask("a", [](ecs::ResMut<Counter> value) { ++value->value; });
    world.AddTask("b", [](ecs::Res<Counter>) {});
    EXPECT_THROW(world.Update(jobs), std::invalid_argument);
    EXPECT_EQ(world.GetResource<Counter>().value, 0);
    ecs::World missing("Missing");
    missing.AddTask("read", [](ecs::Res<Counter>) {});
    EXPECT_THROW(missing.Update(jobs), std::logic_error);
}
TEST(ResourceTest, ResourcesAreWorldScopedAndBorrowingDoesNotTransferOwnership) {
    Counter    external{10};
    ecs::World a("A"), b("B");
    a.BorrowResource(external);
    b.EmplaceResource<Counter>(20);
    a.GetResource<Counter>().value = 30;
    EXPECT_EQ(external.value, 30);
    EXPECT_EQ(b.GetResource<Counter>().value, 20);
    EXPECT_THROW(a.EmplaceResource<Counter>(), std::logic_error);
}
TEST(ResourceTest, MutationDuringExecutionAndAliasedWritesAreRejected) {
    ecs::World      world("Mutation");
    core::JobSystem jobs(core::JobSystem::Execution::CallingThread);
    world.AddTask("mutate", [&] { world.EmplaceResource<Counter>(); });
    EXPECT_THROW(world.Update(jobs), std::logic_error);
    EXPECT_FALSE(world.HasResource<Counter>());
    ecs::World aliases("Aliases");
    aliases.EmplaceResource<Counter>();
    aliases.AddTask("alias", [](ecs::ResMut<Counter>, ecs::Res<Counter>) {});
    EXPECT_THROW(aliases.Update(jobs), std::invalid_argument);
}
TEST(ResourceTest, ResourcesOutliveSystemsAndDestroyInReverseInstallationOrder) {
    std::vector<int> events;
    struct First {
        std::vector<int>& events;
        ~First() { events.push_back(1); }
    };
    struct Second {
        std::vector<int>& events;
        ~Second() { events.push_back(2); }
    };
    struct Consumer {
        std::vector<int>& events;
        void              OnDestroy(ecs::World& world) {
            EXPECT_TRUE(world.HasResource<First>());
            events.push_back(3);
        }
    };
    {
        ecs::World world("Lifetime");
        world.EmplaceResource<First>(events);
        world.EmplaceResource<Second>(events);
        world.RegisterSystem<Consumer>(events);
    }
    EXPECT_EQ(events, (std::vector<int>{3, 2, 1}));
}
