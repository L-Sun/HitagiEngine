#include "test_macros.hpp"
import interop.gtest;
import std;
import core;
import ecs;

using namespace hitagi;

namespace runtime_schedule_test {
// Test-owned executor, including reentrant callbacks used by the lifecycle tests.
auto CallerJobs() -> core::JobSystem& {
    static thread_local core::JobSystem jobs(core::JobSystem::Execution::CallingThread);
    return jobs;
}

enum class Group { First,
                   PreUpdate,
                   Update,
                   PostUpdate,
                   Render,
                   Last };
void OrderGroups(ecs::Schedule& schedule) {
    schedule.SetOrder(Group::First, Group::PreUpdate);
    schedule.SetOrder(Group::PreUpdate, Group::Update);
    schedule.SetOrder(Group::Update, Group::PostUpdate);
    schedule.SetOrder(Group::PostUpdate, Group::Render);
    schedule.SetOrder(Group::Render, Group::Last);
}
}  // namespace runtime_schedule_test
using runtime_schedule_test::Group;

TEST(RuntimeScheduleTest, EmptyWorldRunsBoundTasksOnceInGroupAndExplicitOrder) {
    struct System {
        std::vector<int>& events;
        int               builds = 0;
        void              ConfigureSchedule(ecs::Schedule& schedule) {
            runtime_schedule_test::OrderGroups(schedule);
            ++builds;
            schedule.Request("last", [this] { events.push_back(4); }, Group::Last);
            schedule.Request("second", [this] { events.push_back(2); }, Group::Update);
            schedule.Request("third", [this] { events.push_back(3); }, Group::Update);
            schedule.Request("first", [this] { events.push_back(1); }, Group::First);
            schedule.SetOrder("second", "third");
        }
    };
    std::vector<int> events;
    ecs::World       world("Phases");
    auto&            system = world.RegisterSystem<System>(events);
    EXPECT_EQ(&system, &world.RegisterSystem<System>(events));
    world.Update(runtime_schedule_test::CallerJobs());
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(system.builds, 1);
    EXPECT_EQ(events, (std::vector<int>{1, 2, 3, 4, 1, 2, 3, 4}));
    events.clear();
    core::JobSystem jobs(2);
    world.Update(jobs);
    EXPECT_EQ(events, (std::vector<int>{1, 2, 3, 4}));
}

TEST(RuntimeScheduleTest, CallingThreadExecutionRecoversAfterFailureAndRejectsReentry) {
    struct System {
        ecs::World&     world;
        std::thread::id caller;
        bool            fail  = true;
        unsigned        calls = 0;
        void            ConfigureSchedule(ecs::Schedule& schedule) {
            runtime_schedule_test::OrderGroups(schedule);
            schedule.Request("caller", [this] {
                EXPECT_EQ(std::this_thread::get_id(), caller);
                EXPECT_THROW(world.Update(runtime_schedule_test::CallerJobs()), std::logic_error);
                ++calls;
                if (fail) throw std::runtime_error("frame failed");
            });
        }
    };
    ecs::World world("ThreadAffinity");
    auto&      system = world.RegisterSystem<System>(world, std::this_thread::get_id());
    EXPECT_THROW(world.Update(runtime_schedule_test::CallerJobs()), std::runtime_error);
    system.fail = false;
    EXPECT_NO_THROW(world.Update(runtime_schedule_test::CallerJobs()));
    EXPECT_EQ(system.calls, 2u);
}

TEST(RuntimeScheduleTest, UnregisterDuringExecutionRetainsBoundStateUntilSnapshotRetires) {
    struct System {
        ecs::World& world;
        bool&       destroyed;
        int&        result;
        ~System() { destroyed = true; }
        void ConfigureSchedule(ecs::Schedule& schedule) {
            runtime_schedule_test::OrderGroups(schedule);
            schedule.Request("remove", [this] {
                world.GetSystemManager().Unregister<System>();
                EXPECT_FALSE(destroyed);
            });
            schedule.Request("still-valid", [this] { ++result; });
            schedule.SetOrder("remove", "still-valid");
        }
    };
    bool       destroyed = false;
    int        result    = 0;
    ecs::World world("SnapshotLifetime");
    world.RegisterSystem<System>(world, destroyed, result);
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_FALSE(destroyed);
    EXPECT_EQ(result, 1);
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_TRUE(destroyed);
    EXPECT_EQ(result, 1);
}

TEST(RuntimeScheduleTest, DisableEnableRetainsStateAndRebuildsTasks) {
    struct System {
        int  value = 0;
        void ConfigureSchedule(ecs::Schedule& schedule) {
            runtime_schedule_test::OrderGroups(schedule);
            schedule.Request("increment", [this] { ++value; });
        }
    };
    ecs::World world("EnableState");
    auto&      system = world.RegisterSystem<System>();
    world.Update(runtime_schedule_test::CallerJobs());
    world.GetSystemManager().Disable<System>();
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(system.value, 1);
    world.GetSystemManager().Enable<System>();
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(system.value, 2);
}

TEST(RuntimeScheduleTest, GroupContradictionRejectsGraphBeforeAnyTaskRuns) {
    struct System {
        unsigned& calls;
        void      ConfigureSchedule(ecs::Schedule& schedule) {
            runtime_schedule_test::OrderGroups(schedule);
            schedule.Request("first", [this] { ++calls; }, Group::First);
            schedule.Request("last", [this] { ++calls; }, Group::Last);
            schedule.SetOrder("last", "first");
        }
    };
    unsigned   calls = 0;
    ecs::World world("Contradiction");
    world.RegisterSystem<System>(calls);
    EXPECT_THROW(world.Update(runtime_schedule_test::CallerJobs()), std::invalid_argument);
    core::JobSystem jobs(1);
    EXPECT_THROW(world.Update(jobs), std::invalid_argument);
    EXPECT_EQ(calls, 0u);
}

TEST(RuntimeScheduleTest, ComponentAndOnceTasksShareOneGraphAndCurrentFrameDuration) {
    struct Counter {
        int value = 0;
    };
    struct System {
        int&    result;
        double& elapsed;
        void    ConfigureSchedule(ecs::Schedule& schedule) {
            runtime_schedule_test::OrderGroups(schedule);
            schedule.Request("component", [](ecs::Query<Counter&> query) { query.Each([&](Counter& counter) { ++counter.value; }); }, Group::PreUpdate);
            schedule.Request("read", [this](ecs::Query<const Counter&> query) { query.Each([&](const Counter& counter) { result = counter.value; }); }, Group::PostUpdate);
            schedule.Request("time", [this, &schedule] { elapsed = schedule.GetDeltaSeconds(); }, Group::Last);
        }
    };
    int        result  = 0;
    double     elapsed = 0;
    ecs::World world("MixedTasks");
    world.GetEntityManager().Create().Emplace<Counter>();
    world.RegisterSystem<System>(result, elapsed);
    world.Update(runtime_schedule_test::CallerJobs(), 0.25);
    EXPECT_EQ(result, 1);
    EXPECT_DOUBLE_EQ(elapsed, 0.25);
    world.Update(runtime_schedule_test::CallerJobs(), 0.5);
    EXPECT_EQ(result, 2);
    EXPECT_DOUBLE_EQ(elapsed, 0.5);
}

TEST(RuntimeScheduleTest, FailedCreationDoesNotLeaveARegisteredInstance) {
    struct System {
        bool& fail;
        int&  live;
        System(bool& should_fail, int& count) : fail(should_fail), live(count) { ++live; }
        ~System() { --live; }
        void OnCreate(ecs::World&) {
            if (fail) throw std::runtime_error("creation failed");
        }
    };
    bool fail = true;
    int  live = 0;
    {
        ecs::World world("CreationFailure");
        EXPECT_THROW(world.RegisterSystem<System>(fail, live), std::runtime_error);
        EXPECT_EQ(live, 0);
        fail = false;
        EXPECT_NO_THROW(world.RegisterSystem<System>(fail, live));
        EXPECT_EQ(live, 1);
    }
    EXPECT_EQ(live, 0);
}

TEST(RuntimeScheduleTest, UnknownOrderAndDuplicateNamesAreErrors) {
    struct Unknown {
        void ConfigureSchedule(ecs::Schedule& schedule) {
            runtime_schedule_test::OrderGroups(schedule);
            schedule.Request("exists", [] {});
            schedule.SetOrder("missing", "exists");
        }
    };
    struct Duplicate {
        void ConfigureSchedule(ecs::Schedule& schedule) {
            runtime_schedule_test::OrderGroups(schedule);
            schedule.Request("duplicate", [] {});
            schedule.Request("duplicate", [] {});
        }
    };
    ecs::World world("InvalidNames");
    world.RegisterSystem<Unknown>();
    EXPECT_THROW(world.Update(runtime_schedule_test::CallerJobs()), std::invalid_argument);
    world.GetSystemManager().Unregister<Unknown>();
    world.RegisterSystem<Duplicate>();
    EXPECT_THROW(world.Update(runtime_schedule_test::CallerJobs()), std::invalid_argument);
    world.GetSystemManager().Unregister<Duplicate>();
    EXPECT_NO_THROW(world.Update(runtime_schedule_test::CallerJobs()));
}

TEST(RuntimeScheduleTest, FailedEnableCanBeRetriedWithoutReplacingState) {
    struct System {
        bool& fail;
        int&  calls;
        void  OnEnable(ecs::World&) {
            if (fail) throw std::runtime_error("enable failed");
        }
        void ConfigureSchedule(ecs::Schedule& schedule) {
            runtime_schedule_test::OrderGroups(schedule);
            schedule.Request("run", [this] { ++calls; });
        }
    };
    bool       fail  = true;
    int        calls = 0;
    ecs::World world("EnableFailure");
    EXPECT_THROW(world.RegisterSystem<System>(fail, calls), std::runtime_error);
    fail         = false;
    auto& system = world.RegisterSystem<System>(fail, calls);
    world.Update(runtime_schedule_test::CallerJobs());
    world.GetSystemManager().Disable<System>();
    fail = true;
    EXPECT_THROW(world.GetSystemManager().Enable<System>(), std::runtime_error);
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(calls, 1);
    fail = false;
    EXPECT_EQ(&system, &world.RegisterSystem<System>(fail, calls));
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(calls, 2);
}

TEST(RuntimeScheduleTest, SelfRemovalDuringRegistrationDoesNotReturnDanglingReference) {
    struct System {
        void OnEnable(ecs::World& world) { world.GetSystemManager().Unregister<System>(); }
    };
    ecs::World world("SelfRemoval");
    EXPECT_THROW(world.RegisterSystem<System>(), std::logic_error);
    EXPECT_NO_THROW(world.Update(runtime_schedule_test::CallerJobs()));
}

TEST(RuntimeScheduleTest, EnumTypeAndValueBothIdentifyGroups) {
    enum class First : std::int8_t { A     = -1,
                                     Alias = -1,
                                     B     = 0 };
    enum class Second : std::uint64_t { A = 0xffffffffffffffffULL,
                                        B = 0 };
    EXPECT_EQ(ecs::Group(First::A), ecs::Group(First::Alias));
    EXPECT_NE(ecs::Group(First::A), ecs::Group(Second::A));
    EXPECT_NE(ecs::Group(First::B), ecs::Group(Second::B));
    struct System {
        std::vector<int>& events;
        void              ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("third", [this] { events.push_back(3); }, Second::B);
            schedule.Request("second", [this] { events.push_back(2); }, First::Alias);
            schedule.Request("first", [this] { events.push_back(1); }, Second::A);
            schedule.SetOrder(Second::A, First::A);
            schedule.SetOrder(First::Alias, Second::B);
        }
    };
    std::vector<int> events;
    ecs::World       world("EnumIdentity");
    world.RegisterSystem<System>(events);
    world.Update(runtime_schedule_test::CallerJobs());
    core::JobSystem jobs(2);
    world.Update(jobs);
    EXPECT_EQ(events, (std::vector<int>{1, 2, 3, 1, 2, 3}));
}

TEST(RuntimeScheduleTest, ExplicitGroupsOrderComponentAccess) {
    enum class Group { Before,
                       Write,
                       After };
    struct Counter {
        int value = 0;
    };
    struct System {
        std::vector<int>& events;
        void              ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.SetOrder(Group::Before, Group::Write);
            schedule.SetOrder(Group::Write, Group::After);
            schedule.Request("after", [this](ecs::Query<const Counter&> query) { query.Each([&](const Counter& c) { events.push_back(c.value); }); }, Group::After);
            schedule.Request("write", [](ecs::Query<Counter&> query) { query.Each([&](Counter& c) { ++c.value; }); }, Group::Write);
            schedule.Request("before", [this](ecs::Query<const Counter&> query) { query.Each([&](const Counter& c) {
                                                                                      events.push_back(c.value);
                                                                                  }); }, Group::Before);
        }
    };
    std::vector<int> events;
    ecs::World       world("CrossGroupAccess");
    world.GetEntityManager().Create().Emplace<Counter>();
    world.RegisterSystem<System>(events);
    world.Update(runtime_schedule_test::CallerJobs());
    core::JobSystem jobs(2);
    world.Update(jobs);
    EXPECT_EQ(events, (std::vector<int>{0, 1, 1, 2}));
}

TEST(RuntimeScheduleTest, GroupOrderSelectsEitherReadWriteDirection) {
    enum class Group { Read,
                       Empty,
                       Write };
    struct Counter {
        int value = 0;
    };
    struct System {
        bool before;
        int& calls;
        void ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("write", [this](ecs::Query<Counter&> query) { query.Each([&](Counter& c) { ++c.value; ++calls; }); }, Group::Write);
            if (before) {
                schedule.Request("read", [this](ecs::Query<const Counter&> query) { query.Each([&](const Counter&) { ++calls; }); }, Group::Read);
                // Explicit write before read.
                schedule.SetOrder(Group::Write, Group::Empty);
                schedule.SetOrder(Group::Empty, Group::Read);
            } else {
                schedule.Request("read", [this](ecs::Query<const Counter&> query) { query.Each([&](const Counter&) { ++calls; }); }, Group::Read);
                // Explicit read before write.
                schedule.SetOrder(Group::Read, Group::Empty);
                schedule.SetOrder(Group::Empty, Group::Write);
            }
        }
    };
    for (const bool before : {false, true}) {
        int        calls = 0;
        ecs::World world("AccessContradiction");
        world.GetEntityManager().Create().Emplace<Counter>();
        world.RegisterSystem<System>(before, calls);
        EXPECT_NO_THROW(world.Update(runtime_schedule_test::CallerJobs()));
        core::JobSystem jobs(2);
        EXPECT_NO_THROW(world.Update(jobs));
        EXPECT_EQ(calls, 4);
    }
}

TEST(RuntimeScheduleTest, EmptyGroupsPreserveTransitiveOrderAndRejectCycles) {
    enum class Group { Begin,
                       Empty,
                       End };
    struct System {
        std::vector<int>& events;
        bool              cycle;
        void              ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.SetOrder(Group::Begin, Group::Empty);
            schedule.SetOrder(Group::Empty, Group::End);
            if (cycle) {
                schedule.SetOrder(Group::End, Group::Begin);
            } else {
                // Multiple group members must all precede the later group.
                schedule.Request("end", [this] { events.push_back(2); }, Group::End);
                schedule.Request("begin-a", [this] { events.push_back(1); }, Group::Begin);
                schedule.Request("begin-b", [this] { events.push_back(1); }, Group::Begin);
                schedule.SetOrder("begin-a", "begin-b");
            }
        }
    };
    std::vector<int> events;
    ecs::World       world("EmptyGroups");
    world.RegisterSystem<System>(events, false);
    world.Update(runtime_schedule_test::CallerJobs());
    core::JobSystem jobs(2);
    world.Update(jobs);
    EXPECT_EQ(events, (std::vector<int>{1, 1, 2, 1, 1, 2}));
    ecs::World invalid("EmptyCycle");
    invalid.RegisterSystem<System>(events, true);
    EXPECT_THROW(invalid.Update(runtime_schedule_test::CallerJobs()), std::invalid_argument);
    EXPECT_THROW(invalid.Update(jobs), std::invalid_argument);
}

TEST(RuntimeScheduleTest, ExplicitWriterOrderOverridesDeclarationOrder) {
    enum class Group { First,
                       Second };
    struct Counter {
        int value = 0;
    };
    struct System {
        bool reverse;
        void ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("first", [](ecs::Query<Counter&> query) { query.Each([&](Counter& c) { c.value += 2; }); }, Group::First);
            schedule.Request("second", [](ecs::Query<Counter&> query) { query.Each([&](Counter& c) { c.value *= 3; }); }, Group::Second);
            if (reverse)
                schedule.SetOrder(Group::Second, Group::First);
            else
                schedule.SetOrder(Group::First, Group::Second);
        }
    };
    ecs::World world("WriterOrder");
    auto       entity = world.GetEntityManager().Create();
    entity.Emplace<Counter>();
    world.RegisterSystem<System>(false);
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(entity.Get<Counter>().value, 6);
    core::JobSystem jobs(2);
    world.Update(jobs);
    EXPECT_EQ(entity.Get<Counter>().value, 24);
    world.GetSystemManager().Unregister<System>();
    world.RegisterSystem<System>(true);
    EXPECT_NO_THROW(world.Update(runtime_schedule_test::CallerJobs()));
    EXPECT_EQ(entity.Get<Counter>().value, 74);
}

TEST(RuntimeScheduleTest, QueryRunsOnceWhenEmptyAndCanTraverseMoreThanOnce) {
    struct Value {
        int value = 0;
    };
    struct System {
        int  calls = 0;
        void ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("two traversals", [this](ecs::Query<Value&> query) {
                ++calls;
                query.Each([](Value& value) { ++value.value; });
                query.Each([](Value& value) { value.value *= 3; });
            });
        }
    };
    ecs::World world("QueryTest");
    auto&      system = world.RegisterSystem<System>();
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(system.calls, 1);
    auto entity = world.GetEntityManager().Create();
    entity.Emplace<Value>();
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(system.calls, 2);
    EXPECT_EQ(entity.Get<Value>().value, 3);
}

TEST(RuntimeScheduleTest, ExplicitOrderAllowsReadBetweenTwoWrites) {
    struct Value {
        int value = 0;
    };
    struct System {
        int& observed;
        void ConfigureSchedule(ecs::Schedule& schedule) {
            // Deliberately register in the opposite order.
            schedule.Request("last", [](ecs::Query<Value&> q) { q.Each([](Value& v) { v.value = 9; }); });
            schedule.Request("read", [this](ecs::Query<const Value&> q) { q.Each([this](const Value& v) { observed = v.value; }); });
            schedule.Request("first", [](ecs::Query<Value&> q) { q.Each([](Value& v) { v.value = 4; }); });
            schedule.SetOrder("first", "read");
            schedule.SetOrder("read", "last");
        }
    };
    core::JobSystem pool(2);
    for (auto* jobs : {&runtime_schedule_test::CallerJobs(), &pool}) {
        int        observed = 0;
        ecs::World world("QueryTest");
        auto       e = world.GetEntityManager().Create();
        e.Emplace<Value>();
        world.RegisterSystem<System>(observed);
        world.Update(*jobs);
        EXPECT_EQ(observed, 4);
        EXPECT_EQ(e.Get<Value>().value, 9);
    }
}

TEST(RuntimeScheduleTest, MissingConflictOrderFailsBeforeEvenIndependentWorkRuns) {
    struct Value {
        int value = 0;
    };
    struct System {
        int& calls;
        bool writers;
        void ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("independent", [this] { ++calls; });
            schedule.Request("writer", [](ecs::Query<Value&> q) { q.Each([](Value& v) { ++v.value; }); });
            if (writers)
                schedule.Request("other", [](ecs::Query<Value&>) {});
            else
                schedule.Request("other", [](ecs::Query<const Value&>) {});
        }
    };
    core::JobSystem pool(2);
    for (bool writers : {false, true}) {
        int        calls = 0;
        ecs::World world("QueryTest");
        auto       e = world.GetEntityManager().Create();
        e.Emplace<Value>();
        world.RegisterSystem<System>(calls, writers);
        for (auto* jobs : {&runtime_schedule_test::CallerJobs(), &pool}) {
            try {
                world.Update(*jobs);
                FAIL() << "Unordered conflict was accepted";
            } catch (const std::invalid_argument& error) {
                EXPECT_NE(std::string_view(error.what()).find("writer"), std::string_view::npos);
                EXPECT_NE(std::string_view(error.what()).find("other"), std::string_view::npos);
            }
        }
        EXPECT_EQ(calls, 0);
        EXPECT_EQ(e.Get<Value>().value, 0);
    }
}

TEST(RuntimeScheduleTest, ParallelQueryJoinsBeforeSuccessorAndSupportsSingleWorker) {
    struct Value {
        int value = 0;
    };
    struct System {
        std::atomic<int>& visits;
        int&              total;
        void              ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("write", [this](ecs::Query<Value&> query) {
                query.ParallelEach([this](Value& value) { ++value.value; visits.fetch_add(1); }, 17);
            });
            schedule.Request("read", [this](ecs::Query<const Value&> query) {
                total = 0;
                query.Each([this](const Value& value) { total += value.value; });
            });
            schedule.SetOrder("write", "read");
        }
    };
    core::JobSystem one_worker(1), pool(4);
    for (auto* jobs : {&runtime_schedule_test::CallerJobs(), &one_worker, &pool}) {
        std::atomic<int> visits = 0;
        int              total  = 0;
        ecs::World       world("QueryTest");
        (void)world.GetEntityManager().CreateMany<Value>(2049);
        world.RegisterSystem<System>(visits, total);
        world.Update(*jobs);
        EXPECT_EQ(visits.load(), 2049);
        EXPECT_EQ(total, 2049);
        world.Update(*jobs);
        EXPECT_EQ(total, 4098);
    }
}

TEST(RuntimeScheduleTest, ParallelQueryFailureJoinsAndAllowsNextUpdate) {
    struct Value {
        int value = 0;
    };
    struct System {
        bool             fail = true;
        std::atomic<int> active{0};
        int              completed = 0;
        void             ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("work", [this](ecs::Query<Value&> query) {
                query.ParallelEach([this](Value& value) {
                    struct Guard {
                        std::atomic<int>& active;
                        ~Guard() { --active; }
                    } guard{active};
                    ++active;
                    if (fail) throw std::runtime_error("query failure");
                    ++value.value;
                },
                                   4);
            });
            schedule.Request("done", [this] { ++completed; });
            schedule.SetOrder("work", "done");
        }
    };
    core::JobSystem one_worker(1), pool(4);
    for (auto* jobs : {&runtime_schedule_test::CallerJobs(), &one_worker, &pool}) {
        ecs::World world("QueryTest");
        (void)world.GetEntityManager().CreateMany<Value>(100);
        auto& system = world.RegisterSystem<System>();
        EXPECT_THROW(world.Update(*jobs), std::runtime_error);
        EXPECT_EQ(system.active.load(), 0);
        EXPECT_EQ(system.completed, 0);
        system.fail = false;
        EXPECT_NO_THROW(world.Update(*jobs));
        EXPECT_EQ(system.completed, 1);
    }
}

TEST(RuntimeScheduleTest, StructuralMutationIsRejectedDuringQueryAndGuardRecovers) {
    struct Value {
        int value = 0;
    };
    struct Added {
        int value = 0;
    };
    struct System {
        ecs::World& world;
        void        ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("query", [this](ecs::Query<ecs::Entity, Value&> query) {
                query.Each([this](ecs::Entity entity, Value&) {
                    EXPECT_THROW((void)world.GetEntityManager().Create(), std::logic_error);
                    EXPECT_THROW(entity.Emplace<Added>(), std::logic_error);
                    EXPECT_THROW(entity.Remove<Value>(), std::logic_error);
                    EXPECT_THROW(world.GetEntityManager().Destroy(entity), std::logic_error);
                });
            });
        }
    };
    ecs::World world("QueryTest");
    auto       entity = world.GetEntityManager().Create();
    entity.Emplace<Value>();
    world.RegisterSystem<System>(world);
    core::JobSystem pool(2);
    world.Update(pool);
    EXPECT_NO_THROW(entity.Emplace<Added>());
    EXPECT_NO_THROW(world.GetEntityManager().Destroy(entity));
}

TEST(RuntimeScheduleTest, ReadOnlyQueriesCanOverlapOnWorkers) {
    struct Value {
        int value = 7;
    };
    struct System {
        std::mutex              mutex;
        std::condition_variable ready;
        unsigned                entered = 0;
        void                    Read(ecs::Query<const Value&> query) {
            std::unique_lock lock(mutex);
            ++entered;
            ready.notify_all();
            EXPECT_TRUE(ready.wait_for(lock, std::chrono::seconds(2), [this] { return entered == 2; }));
            lock.unlock();
            query.Each([](const Value& value) { EXPECT_EQ(value.value, 7); });
        }
        void ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("a", [this](ecs::Query<const Value&> query) { Read(std::move(query)); });
            schedule.Request("b", [this](ecs::Query<const Value&> query) { Read(std::move(query)); });
        }
    };
    ecs::World world("Parallel readers");
    world.GetEntityManager().Create().Emplace<Value>();
    world.RegisterSystem<System>();
    core::JobSystem jobs(2);
    world.Update(jobs);
}

TEST(RuntimeScheduleTest, ParallelQueryBatchesActuallyOverlap) {
    struct Value {
        int value = 0;
    };
    struct System {
        std::mutex              mutex;
        std::condition_variable ready;
        unsigned                entered = 0;
        void                    ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("parallel", [this](ecs::Query<Value&> query) {
                query.ParallelEach([this](Value& value) {
                    std::unique_lock lock(mutex);
                    ++entered;
                    ready.notify_all();
                    EXPECT_TRUE(ready.wait_for(lock, std::chrono::seconds(2), [this] { return entered == 2; }));
                    ++value.value;
                },
                                   1);
            });
        }
    };
    ecs::World world("Parallel query batches");
    const auto entities = world.GetEntityManager().CreateMany<Value>(2);
    world.RegisterSystem<System>();
    core::JobSystem jobs(2);
    world.Update(jobs);
    for (auto entity : entities) EXPECT_EQ(entity.Get<Value>().value, 1);
}

TEST(RuntimeScheduleTest, HostConfigurationSurvivesRegistryRebuilds) {
    struct Producer {
        std::vector<int>& events;
        void              ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("produce", [this] { events.push_back(1); }, Group::Update);
        }
    };
    struct Consumer {
        std::vector<int>& events;
        void              ConfigureSchedule(ecs::Schedule& schedule) {
            schedule.Request("consume", [this] { events.push_back(2); }, Group::Render);
        }
    };
    int              builds = 0;
    std::vector<int> events;
    ecs::World       world("HostConfiguration", [&](ecs::Schedule& schedule) {
        ++builds;
        runtime_schedule_test::OrderGroups(schedule);
    });
    // Register in reverse execution order; intermediate groups have no tasks.
    world.RegisterSystem<Consumer>(events);
    world.RegisterSystem<Producer>(events);
    world.Update(runtime_schedule_test::CallerJobs());
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(builds, 1);
    EXPECT_EQ(events, (std::vector<int>{1, 2, 1, 2}));
    events.clear();
    world.GetSystemManager().Disable<Producer>();
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(builds, 2);
    EXPECT_EQ(events, (std::vector<int>{2}));
    events.clear();
    world.GetSystemManager().Enable<Producer>();
    core::JobSystem jobs(2);
    world.Update(jobs);
    EXPECT_EQ(builds, 3);
    EXPECT_EQ(events, (std::vector<int>{1, 2}));
}

TEST(RuntimeScheduleTest, FailedHostConfigurationIsDiscardedAndRetried) {
    int        attempts = 0;
    int        calls    = 0;
    ecs::World world("HostConfigurationRetry", [&](ecs::Schedule& schedule) {
        schedule.Request("host task", [&] { ++calls; });
        if (++attempts == 1) throw std::runtime_error("configuration failed");
    });
    EXPECT_THROW(world.Update(runtime_schedule_test::CallerJobs()), std::runtime_error);
    EXPECT_EQ(calls, 0);
    EXPECT_NO_THROW(world.Update(runtime_schedule_test::CallerJobs()));
    EXPECT_EQ(attempts, 2);
    EXPECT_EQ(calls, 1);
    world.Update(runtime_schedule_test::CallerJobs());
    EXPECT_EQ(attempts, 2);
    EXPECT_EQ(calls, 2);
}
