#include "test_macros.hpp"
import interop.gtest;

import std;
import core;
import ecs;
import math;
import test_utils;

using namespace hitagi::ecs;
using namespace hitagi::math;
namespace core = hitagi::core;

template <Component T, typename V>
auto component_value_eq(const char* expr_entity, const char*, Entity entity, const V& value) -> ::testing::AssertionResult {
    static_assert(std::is_same_v<decltype(T::value), V>);

    if (auto component_value = entity.Get<T>().value; component_value == value) {
        return ::testing::AssertionSuccess();
    } else {
        return ::testing::AssertionFailure() << std::format("Expect component value of {} is {}, but actual is {}", expr_entity, value, component_value);
    }
}

#define EXPECT_COMPONENT_EQ(_entity, _component, _value) \
    EXPECT_PRED_FORMAT2(component_value_eq<_component>, _entity, _value)

auto dynamic_component_value_eq(const char* expr_entity,
                                const char* expr_dynamic_component,
                                const char*,
                                Entity           entity,
                                std::string_view dynamic_component,
                                int              value) -> ::testing::AssertionResult {
    auto component = entity.Get(dynamic_component);
    if (component) {
        if (auto component_value = *reinterpret_cast<int*>(component); component_value == value) {
            return ::testing::AssertionSuccess();
        } else {
            return ::testing::AssertionFailure() << std::format("Expect component value of entity({}) is {}, but actual is {}", expr_entity, value, component_value);
        }
    } else {
        return testing::AssertionFailure() << std::format("The Entity({}) does not have component({})\n", expr_entity, expr_dynamic_component);
    }
}

#define EXPECT_DYNAMIC_COMPONENT_EQ(_entity, _dynamic_component, _value) \
    EXPECT_PRED_FORMAT3(dynamic_component_value_eq, _entity, _dynamic_component, _value)

struct Component_1 {
    int value = 1;
};
struct Component_2 {
    int value = 2;
};
struct Component_3 {
    int value = 3;
};
struct ContainerComponent {
    std::string value;
};

class EcsTest : public ::testing::Test {
public:
    EcsTest()
        : world(::testing::UnitTest::GetInstance()->current_test_info()->name()),
          em(world.GetEntityManager()),
          sm(world.GetSystemManager()) {}
    core::JobSystem job_system;  // executor for the parallel Update(job_system) path
    World           world;
    EntityManager&  em;
    SystemManager&  sm;
};

TEST_F(EcsTest, FailedComponentConstructionPreservesEntityAndNeighbours) {
    static int live = 0;
    live            = 0;
    struct Fragile {
        explicit Fragile(bool fail) {
            if (fail) throw std::runtime_error("construction failed");
            ++live;
        }
        Fragile(const Fragile&) noexcept { ++live; }
        ~Fragile() { --live; }
    };
    auto entities                               = em.CreateMany<ContainerComponent>(3);
    entities[0].Get<ContainerComponent>().value = "unchanged";
    entities[1].Emplace<Fragile>(false);
    EXPECT_THROW(entities[0].Emplace<Fragile>(true), std::runtime_error);
    EXPECT_FALSE(entities[0].Has<Fragile>());
    EXPECT_EQ(entities[0].Get<ContainerComponent>().value, "unchanged");
    EXPECT_EQ(entities[1].Get<Entity>(), entities[1]);
    EXPECT_EQ(live, 1);
    entities[0].Emplace<Fragile>(false);
    EXPECT_EQ(live, 2);
    for (auto& entity : entities) em.Destroy(entity);
    EXPECT_EQ(live, 0);
}

TEST_F(EcsTest, FailedBatchCreationRollsBackConstructedRows) {
    static int live      = 0;
    static int remaining = 100;
    live                 = 0;
    remaining            = 100;
    struct Fragile {
        Fragile() {
            if (--remaining == 0) throw std::runtime_error("batch failed");
            ++live;
        }
        Fragile(const Fragile&) noexcept { ++live; }
        ~Fragile() { --live; }
    };
    auto existing = em.CreateMany<Fragile>(1).front();
    remaining     = 3;
    EXPECT_THROW((void)em.CreateMany<Fragile>(8), std::runtime_error);
    EXPECT_EQ(em.NumEntities(), 1);
    EXPECT_EQ(live, 1);
    EXPECT_TRUE(existing.Valid());
    EXPECT_EQ(existing.Get<Entity>(), existing);
    remaining        = 100;
    auto replacement = em.CreateMany<Fragile>(2);
    EXPECT_EQ(live, 3);
    em.Destroy(existing);
    for (auto& entity : replacement) em.Destroy(entity);
    EXPECT_EQ(live, 0);
}

TEST_F(EcsTest, FailedDynamicConstructionLeavesOldComponentsIntact) {
    em.RegisterDynamicComponent({
        .name                = "throws",
        .size                = sizeof(int),
        .default_constructor = [](std::byte*) { throw std::runtime_error("dynamic construction failed"); },
    });
    auto entity                            = em.CreateMany<ContainerComponent>(1).front();
    entity.Get<ContainerComponent>().value = "keep me";
    EXPECT_THROW(entity.Add("throws"), std::runtime_error);
    EXPECT_FALSE(entity.Has("throws"));
    EXPECT_EQ(entity.Get<ContainerComponent>().value, "keep me");
    EXPECT_EQ(em.NumEntities(), 1);
    entity.Emplace<Component_1>();
    EXPECT_EQ(entity.Get<ContainerComponent>().value, "keep me");
}

TEST_F(EcsTest, RawDynamicComponentsSurviveMigrationAndCompaction) {
    em.RegisterDynamicComponent({.name = "raw", .size = sizeof(int), .alignment = alignof(int)});
    const auto* description = &em.GetDynamicComponentInfo("raw");
    auto        entities    = em.CreateMany<Component_1>(320, {"raw"});
    for (std::size_t index = 0; index < entities.size(); ++index) {
        EXPECT_EQ(*reinterpret_cast<int*>(entities[index].Get("raw")), 0);
        *reinterpret_cast<int*>(entities[index].Get("raw")) = static_cast<int>(index + 100);
    }
    // Rehashing the registry must not invalidate archetype column descriptions.
    for (int index = 0; index < 200; ++index)
        em.RegisterDynamicComponent({.name = std::pmr::string(std::format("extra-{}", index)), .size = 1});
    EXPECT_EQ(&em.GetDynamicComponentInfo("raw"), description);
    for (std::size_t index = 0; index < entities.size(); ++index) {
        entities[index].Emplace<Component_2>();
        entities[index].Remove<Component_2>();
        EXPECT_EQ(*reinterpret_cast<int*>(entities[index].Get("raw")), index + 100);
    }
    for (std::size_t index = 1; index < entities.size(); index += 2) em.Destroy(entities[index]);
    for (std::size_t index = 0; index < entities.size(); index += 2)
        EXPECT_EQ(*reinterpret_cast<int*>(entities[index].Get("raw")), index + 100);
}

TEST_F(EcsTest, LargeAndOverAlignedComponentsHaveValidLayouts) {
    struct alignas(256) Large {
        std::array<std::byte, 4096> payload{};
    };
    auto entities = em.CreateMany<Large>(3);
    for (std::size_t index = 0; index < entities.size(); ++index) {
        auto& value = entities[index].Get<Large>();
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&value) % alignof(Large), 0);
        value.payload.back() = static_cast<std::byte>(index + 1);
        entities[index].Emplace<Component_1>();
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&entities[index].Get<Large>()) % alignof(Large), 0);
        EXPECT_EQ(entities[index].Get<Large>().payload.back(), static_cast<std::byte>(index + 1));
    }
    em.Destroy(entities.front());
    EXPECT_EQ(entities.back().Get<Large>().payload.back(), std::byte{3});
}

TEST_F(EcsTest, InvalidDynamicLayoutsAreRejectedAtRegistration) {
    EXPECT_THROW(em.RegisterDynamicComponent({.name = "zero", .size = 0}), std::invalid_argument);
    EXPECT_THROW(em.RegisterDynamicComponent({.name = "alignment", .size = 12, .alignment = 3}), std::invalid_argument);
    EXPECT_THROW(em.RegisterDynamicComponent({.name = "stride", .size = 3, .alignment = 4}), std::invalid_argument);
    em.RegisterDynamicComponent({.name = "valid", .size = 4, .alignment = 4});
    EXPECT_THROW(em.RegisterDynamicComponent({.name = "valid", .size = 8, .alignment = 4}), std::invalid_argument);
}

TEST_F(EcsTest, CustomOrderSupportsMultipleSuccessorsAndDuplicateEdges) {
    static std::atomic<int> phase = 0;
    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request("A", [](const Component_1&) { phase.store(1); });
            schedule.Request("B", [](const Component_1&) { EXPECT_EQ(phase.load(), 1); });
            schedule.Request("C", [](const Component_1&) { EXPECT_EQ(phase.load(), 1); });
            schedule.SetOrder("A", "B");
            schedule.SetOrder("A", "C");
            schedule.SetOrder("A", "C");
        }
    };
    (void)em.CreateMany<Component_1>(1);
    world.RegisterSystem<System>();
    for (int index = 0; index < 8; ++index) {
        phase = 0;
        world.Update();
        phase = 0;
        world.Update(job_system);
    }
}

TEST_F(EcsTest, CyclicSchedulesAreRejectedBeforeEitherExecutorRuns) {
    static int calls = 0;
    calls            = 0;
    struct Cyclic {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request("A", [](const Component_1&) { ++calls; });
            schedule.Request("B", [](const Component_1&) { ++calls; });
            schedule.Request("independent", [](const Component_1&) { ++calls; });
            schedule.SetOrder("A", "B");
            schedule.SetOrder("B", "A");
        }
    };
    struct Valid {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request("valid", [](const Component_1&) { ++calls; });
        }
    };
    (void)em.CreateMany<Component_1>(1);
    world.RegisterSystem<Cyclic>();
    EXPECT_THROW(world.Update(), std::invalid_argument);
    EXPECT_THROW(world.Update(job_system), std::invalid_argument);
    EXPECT_EQ(calls, 0);
    sm.Unregister<Cyclic>();
    world.RegisterSystem<Valid>();
    world.Update();
    world.Update(job_system);
    EXPECT_EQ(calls, 2);
}

TEST_F(EcsTest, DynamicParameterCountAndAccessAliasesAreValidated) {
    struct TooFew {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request("bad", [](std::byte*) {});
        }
    };
    struct TooMany {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request("bad", [](std::byte*) {}, {"one", "two"});
        }
    };
    struct Conflicting {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request("bad", [](Component_1&, const Component_1&) {});
        }
    };
    world.RegisterSystem<TooFew>();
    EXPECT_THROW(world.Update(), std::invalid_argument);
    sm.Unregister<TooFew>();
    world.RegisterSystem<TooMany>();
    EXPECT_THROW(world.Update(), std::invalid_argument);
    sm.Unregister<TooMany>();
    world.RegisterSystem<Conflicting>();
    EXPECT_THROW(world.Update(), std::invalid_argument);
}

TEST_F(EcsTest, ChunkQueriesReevaluateFiltersAndSupportCallableOwnership) {
    static bool enabled = false;
    static int  visited = 0;
    enabled             = false;
    visited             = 0;
    struct System {
        static void OnUpdate(Schedule& schedule) {
            auto increment = [](Component_1& value) { ++value.value; };
            schedule.Request("lvalue-callable", increment);
            schedule.Request("move-only-callable", [amount = std::make_unique<int>(3)](Component_1& value) {
                value.value += *amount;
            });
            schedule.Request("filtered", [](const Component_1&, const ContainerComponent&) { ++visited; }, {}, [](const ComponentChecker&) { return enabled; });
        }
    };
    auto small = em.CreateMany<Component_1>(100);
    auto large = em.CreateMany<Component_1, ContainerComponent>(320);
    world.RegisterSystem<System>();
    world.Update();
    EXPECT_EQ(visited, 0);
    enabled = true;
    world.Update(job_system);
    EXPECT_EQ(visited, 320);
    for (const auto entity : small) EXPECT_EQ(entity.Get<Component_1>().value, 9);
    for (const auto entity : large) EXPECT_EQ(entity.Get<Component_1>().value, 9);
    // The cached Schedule must discover newly-created archetypes and chunks.
    (void)em.CreateMany<Component_1, ContainerComponent, Component_2>(75);
    world.Update();
    EXPECT_EQ(visited, 320 + 395);
}

TEST_F(EcsTest, DynamicParameterTraitsPreserveReadWriteOrdering) {
    static std::vector<int> values;
    values.clear();
    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request("after", [](const std::byte* data) { values.push_back(*reinterpret_cast<const int*>(data)); }, {"raw"});
            schedule.Request("writer", [](std::byte* first, std::byte* alias) {
                EXPECT_EQ(first, alias);
                ++*reinterpret_cast<int*>(first);
            },
                             {"raw", "raw"});
            schedule.Request("before", [](LastFrame<const std::byte*> data) {
                values.push_back(*reinterpret_cast<const int*>(data.value));
            },
                             {"raw"});
        }
    };
    em.RegisterDynamicComponent({.name = "raw", .size = sizeof(int)});
    auto entity = em.CreateMany<>(1, {"raw"}).front();
    world.RegisterSystem<System>();
    world.Update();
    world.Update(job_system);
    EXPECT_EQ(values, (std::vector<int>{0, 1, 1, 2}));
    EXPECT_EQ(*reinterpret_cast<int*>(entity.Get("raw")), 2);
}

TEST_F(EcsTest, ComponentSignaturesIgnoreDeclarationOrder) {
    static int matches = 0;
    static int visits  = 0;
    matches            = 0;
    visits             = 0;
    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request("visit", [](const Component_1&, const Component_2&) { ++visits; }, {}, [](const ComponentChecker&) { ++matches; return true; });
        }
    };
    (void)em.CreateMany<Component_1, Component_2>(2);
    (void)em.CreateMany<Component_2, Component_1>(3);
    world.RegisterSystem<System>();
    world.Update();
    EXPECT_EQ(matches, 1);
    EXPECT_EQ(visits, 5);
}

TEST_F(EcsTest, CreateEntity) {
    const auto entity = em.Create();
    EXPECT_TRUE(entity.Valid());
    EXPECT_TRUE(em.Has(entity));
}

TEST_F(EcsTest, EntityHandlesAreScopedToTheirStorage) {
    World other("OtherStorage");
    auto  local   = em.Create();
    auto  foreign = other.GetEntityManager().Create();
    ASSERT_EQ(local.GetId(), foreign.GetId());
    EXPECT_NE(local, foreign);
    EXPECT_FALSE(em.Has(foreign));
    EXPECT_THROW(em.Destroy(foreign), std::out_of_range);
    EXPECT_TRUE(local.Valid());
    EXPECT_TRUE(foreign.Valid());
}

TEST_F(EcsTest, CompactionAndMigrationPreserveHandlesAcrossChunks) {
    auto entities = em.CreateMany<Component_1, ContainerComponent>(320);
    for (std::size_t index = 0; index < entities.size(); ++index) {
        entities[index].Get<Component_1>().value        = static_cast<int>(index);
        entities[index].Get<ContainerComponent>().value = std::format("entity-{}-nontrivial-component", index);
    }
    // Move rows between archetypes and fill holes from other chunks.
    for (std::size_t index = 0; index < entities.size(); index += 3) {
        entities[index].Emplace<Component_2>();
        entities[index].Remove<Component_2>();
    }
    for (std::size_t index = 1; index < entities.size(); index += 2) em.Destroy(entities[index]);
    EXPECT_EQ(em.NumEntities(), 160);
    for (std::size_t index = 0; index < entities.size(); index += 2) {
        auto entity = entities[index];
        EXPECT_TRUE(entity.Valid());
        EXPECT_EQ(entity.Get<Entity>(), entity);
        EXPECT_EQ(entity.Get<Component_1>().value, index);
        EXPECT_EQ(entity.Get<ContainerComponent>().value, std::format("entity-{}-nontrivial-component", index));
    }
    for (std::size_t index = 0; index < entities.size(); index += 2) em.Destroy(entities[index]);
    EXPECT_EQ(em.NumEntities(), 0);
    // Reuse the now-empty archetype/chunks without reusing old entity IDs.
    auto replacement = em.CreateMany<Component_1, ContainerComponent>(1).front();
    EXPECT_TRUE(replacement.Valid());
    EXPECT_EQ(replacement.Get<Entity>(), replacement);
}

TEST_F(EcsTest, DynamicMigrationReleasesOldRowsExactlyOnce) {
    static int live_components = 0;
    live_components            = 0;
    struct Tracked {
        Tracked() { ++live_components; }
        Tracked(const Tracked&) { ++live_components; }
        Tracked(Tracked&&) noexcept { ++live_components; }
        ~Tracked() { --live_components; }
    };
    {
        World storage_world("DynamicMigration");
        auto& manager = storage_world.GetEntityManager();
        manager.RegisterDynamicComponent({.name = "tag", .size = sizeof(int)});
        auto entities = manager.CreateMany<Tracked>(320);
        ASSERT_EQ(live_components, 320);
        for (auto entity : entities) {
            entity.Add("tag");
            EXPECT_EQ(live_components, 320);
            EXPECT_EQ(entity.Get<Entity>(), entity);
        }
        for (auto entity : entities) {
            entity.Remove("tag");
            EXPECT_EQ(live_components, 320);
        }
        for (auto& entity : entities) manager.Destroy(entity);
        EXPECT_EQ(live_components, 0);
    }
    EXPECT_EQ(live_components, 0);
}

TEST_F(EcsTest, WorldBindsLifecycleAndRebuildsOnlyAfterSystemChanges) {
    static std::vector<std::string> events;
    static int                      builds      = 0;
    static World*                   bound_world = nullptr;
    events.clear();
    builds      = 0;
    bound_world = &world;
    struct System {
        static void OnCreate(World& context) {
            EXPECT_EQ(&context, bound_world);
            events.emplace_back("create");
        }
        static void OnEnable(World&) { events.emplace_back("enable"); }
        static void OnDisable(World&) { events.emplace_back("disable"); }
        static void OnDestroy(World&) { events.emplace_back("destroy"); }
        static void OnUpdate(Schedule& schedule) {
            ++builds;
            schedule.Request("increment", [](Component_1& value) { ++value.value; });
        }
    };
    auto entity = em.CreateMany<Component_1>(1).front();
    world.RegisterSystem<System>();
    world.RegisterSystem<System>();
    world.Update();
    world.Update(job_system);
    EXPECT_EQ(builds, 1);
    EXPECT_COMPONENT_EQ(entity, Component_1, 3);
    sm.Disable<System>();
    sm.Disable<System>();
    world.Update();
    EXPECT_COMPONENT_EQ(entity, Component_1, 3);
    sm.Enable<System>();
    world.Update(job_system);
    EXPECT_EQ(builds, 2);
    EXPECT_COMPONENT_EQ(entity, Component_1, 4);
    sm.Unregister<System>();
    sm.Unregister<System>();
    world.Update();
    EXPECT_COMPONENT_EQ(entity, Component_1, 4);
    EXPECT_EQ(events, (std::vector<std::string>{"create", "enable", "disable", "enable", "disable", "destroy"}));
}

TEST_F(EcsTest, DisablingDuringExecutionDoesNotDestroyRunningSchedule) {
    static World*             context  = nullptr;
    static bool               finished = false;
    static std::weak_ptr<int> task_capture;
    context  = &world;
    finished = false;
    struct System {
        static void OnUpdate(Schedule& schedule) {
            auto lifetime = std::make_shared<int>(42);
            task_capture  = lifetime;
            schedule.Request("disable-self", [lifetime](Component_1& value) {
                context->GetSystemManager().Disable<System>();
                EXPECT_FALSE(task_capture.expired());
                value.value = *lifetime;
                finished    = true;
            });
        }
    };
    auto entity = em.CreateMany<Component_1>(1).front();
    world.RegisterSystem<System>();
    world.Update();
    EXPECT_TRUE(finished);
    EXPECT_COMPONENT_EQ(entity, Component_1, 42);
    EXPECT_FALSE(task_capture.expired());
    world.Update();
    EXPECT_TRUE(task_capture.expired());
}

TEST_F(EcsTest, RegistrationDuringScheduleBuildTakesEffectNextUpdate) {
    static World* context = nullptr;
    context               = &world;
    struct AddedSystem {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request("added", [](Component_1& value) { value.value += 10; });
        }
    };
    struct RegisteringSystem {
        static void OnUpdate(Schedule& schedule) {
            context->RegisterSystem<AddedSystem>();
            schedule.Request("existing", [](Component_1& value) { ++value.value; });
        }
    };
    auto entity = em.CreateMany<Component_1>(1).front();
    world.RegisterSystem<RegisteringSystem>();
    world.Update();
    EXPECT_COMPONENT_EQ(entity, Component_1, 2);
    world.Update();
    EXPECT_COMPONENT_EQ(entity, Component_1, 13);
    world.Update(job_system);
    EXPECT_COMPONENT_EQ(entity, Component_1, 24);
}

TEST_F(EcsTest, ShutdownCallbacksCanStillAccessEntityStorage) {
    static Entity           owned_entity;
    static std::vector<int> shutdown_values;
    shutdown_values.clear();
    struct System {
        static void OnCreate(World& context) {
            owned_entity = context.GetEntityManager().CreateMany<Component_1>(1).front();
        }
        static void OnDisable(World&) { shutdown_values.push_back(owned_entity.Get<Component_1>().value); }
        static void OnDestroy(World& context) {
            shutdown_values.push_back(owned_entity.Get<Component_1>().value);
            context.GetEntityManager().Destroy(owned_entity);
        }
    };
    {
        World lifecycle_world("ShutdownOrder");
        lifecycle_world.RegisterSystem<System>();
    }
    EXPECT_EQ(shutdown_values, (std::vector<int>{1, 1}));
    EXPECT_FALSE(owned_entity);
}

TEST_F(EcsTest, RecursiveUpdateIsRejectedWithoutInvalidatingTheSchedule) {
    static World* context = nullptr;
    context               = &world;
    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request("recursive-update", [](Component_1& value) {
                EXPECT_THROW(context->Update(), std::logic_error);
                ++value.value;
            });
        }
    };
    auto entity = em.CreateMany<Component_1>(1).front();
    world.RegisterSystem<System>();
    world.Update();
    world.Update();
    EXPECT_COMPONENT_EQ(entity, Component_1, 3);
}

TEST_F(EcsTest, CreateManyEntities) {
    const auto entities = em.CreateMany(100);
    ASSERT_EQ(entities.size(), 100);
    EXPECT_EQ(em.NumEntities(), 100);
    for (const auto entity : entities) {
        EXPECT_TRUE(entity.Valid());
        EXPECT_TRUE(em.Has(entity));
    }
}

TEST_F(EcsTest, CreateManyEntitiesWithComponent) {
    em.RegisterDynamicComponent({
        .name                = "DynamicComponent",
        .size                = sizeof(int),
        .default_constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; },
        .destructor          = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; },
    });

    const auto entities = em.CreateMany<Component_1, Component_2>(100, {"DynamicComponent"});
    ASSERT_EQ(entities.size(), 100);
    EXPECT_EQ(em.NumEntities(), 100);
    for (const auto entity : entities) {
        EXPECT_TRUE(entity.Valid());
        EXPECT_TRUE(em.Has(entity));
        EXPECT_COMPONENT_EQ(entity, Component_1, 1);
        EXPECT_COMPONENT_EQ(entity, Component_2, 2);
        EXPECT_DYNAMIC_COMPONENT_EQ(entity, "DynamicComponent", 1);
    }
}

TEST_F(EcsTest, DestroyEntity) {
    auto entity = em.Create();
    em.Destroy(entity);
    EXPECT_FALSE(entity.Valid());
    EXPECT_FALSE(em.Has(entity));
}

TEST_F(EcsTest, DestroyRepeatedly) {
    auto entity = em.Create();
    EXPECT_TRUE(entity);
    EXPECT_EQ(em.NumEntities(), 1);
    em.Destroy(entity);
    EXPECT_EQ(em.NumEntities(), 0);
    EXPECT_FALSE(entity.Valid());
    EXPECT_THROW(em.Destroy(entity), std::out_of_range);
}

TEST_F(EcsTest, DestroyNonExistentEntity) {
    Entity invalid_entity{};
    EXPECT_THROW(em.Destroy(invalid_entity), std::out_of_range);
}

TEST_F(EcsTest, AddComponent) {
    em.RegisterDynamicComponent({
        .name                = "DynamicComponent",
        .size                = sizeof(int),
        .default_constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; },
        .destructor          = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; },
    });

    auto entity = em.Create();
    entity.Emplace<Component_1>();
    entity.Emplace<Component_2>();
    entity.Emplace<Component_3>();
    entity.Add("DynamicComponent");

    EXPECT_COMPONENT_EQ(entity, Component_1, 1);
    EXPECT_COMPONENT_EQ(entity, Component_2, 2);
    EXPECT_COMPONENT_EQ(entity, Component_3, 3);
    EXPECT_DYNAMIC_COMPONENT_EQ(entity, "DynamicComponent", 1);
}

TEST_F(EcsTest, AddExistedComponent) {
    auto entity                         = em.Create();
    entity.Emplace<Component_1>().value = 2;
    entity.Emplace<Component_1>();
    EXPECT_COMPONENT_EQ(entity, Component_1, 2) << "The old component should not be replaced";
}

TEST_F(EcsTest, AddComponentKeepOldComponent) {
    struct PointToSelfComponent {
        PointToSelfComponent() : self(this) {}
        PointToSelfComponent(const PointToSelfComponent&) : self(this) {}
        PointToSelfComponent(PointToSelfComponent&&) noexcept : self(this) {}
        PointToSelfComponent& operator=(const PointToSelfComponent&) { return *this; }
        PointToSelfComponent& operator=(const PointToSelfComponent&&) noexcept { return *this; }
        ~PointToSelfComponent() { self = nullptr; }

        bool IsValid() const noexcept { return self == this; }

        PointToSelfComponent* self = nullptr;
    };

    auto entity = em.Create();
    entity.Emplace<PointToSelfComponent>();
    entity.Emplace<Component_1>();
    EXPECT_TRUE(entity.Has<PointToSelfComponent>());
    EXPECT_TRUE(entity.Has<Component_1>());
    EXPECT_TRUE(entity.Get<PointToSelfComponent>().IsValid());

    auto entity_2                                = em.Create();
    entity_2.Emplace<ContainerComponent>().value = "test";
    entity_2.Emplace<Component_1>();
    EXPECT_STREQ(entity_2.Get<ContainerComponent>().value.c_str(), "test");
}

TEST_F(EcsTest, GetComponent) {
    auto entity = em.Create();
    entity.Emplace<Component_1>();

    ASSERT_TRUE(entity.Has<Entity>()) << "any entity should always have Entity component";
    EXPECT_EQ(entity.Get<Entity>(), entity) << "Entity component should always be the entity itself";
}

TEST_F(EcsTest, GetNotExistedComponent) {
    auto entity = em.Create();
    EXPECT_FALSE(entity.Has<Component_1>());
}

TEST_F(EcsTest, ModifyComponent) {
    auto entity = em.Create();

    entity.Emplace<Component_1>().value = 1;
    EXPECT_COMPONENT_EQ(entity, Component_1, 1);
    entity.Get<Component_1>().value = 2;
    EXPECT_COMPONENT_EQ(entity, Component_1, 2);

    em.RegisterDynamicComponent({
        .name                = "DynamicComponent",
        .size                = sizeof(int),
        .default_constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 2; },
        .destructor          = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; },
    });

    entity.Add("DynamicComponent");
    EXPECT_DYNAMIC_COMPONENT_EQ(entity, "DynamicComponent", 2);
}

TEST_F(EcsTest, RemoveComponent) {
    bool is_destructed = false;

    em.RegisterDynamicComponent({
        .name       = "DynamicComponent",
        .size       = sizeof(int),
        .destructor = [&](std::byte*) { is_destructed = true; },
    });

    auto entity = em.Create();
    entity.Emplace<Component_1>();
    entity.Add("DynamicComponent");

    entity.Remove<Component_1>();
    entity.Remove("DynamicComponent");
    EXPECT_FALSE(entity.Has<Component_1>());
    EXPECT_FALSE(entity.Has("DynamicComponent"));
    EXPECT_TRUE(is_destructed);
}

TEST_F(EcsTest, DestructComponentAfterWorldDestroyed) {
    bool is_destructed = false;
    {
        World _world("DestructComponentAfterWorldDestroyed");
        _world.GetEntityManager().RegisterDynamicComponent({
            .name       = "DynamicComponent",
            .size       = sizeof(int),
            .destructor = [&](std::byte*) { is_destructed = true; },
        });
        auto entity = _world.GetEntityManager().Create();
        entity.Add("DynamicComponent");
    }
    EXPECT_TRUE(is_destructed);
}

TEST_F(EcsTest, RemoveNotExistedComponent) {
    auto entity = em.Create();
    EXPECT_NO_THROW(entity.Remove<Component_1>());
}

TEST_F(EcsTest, Register) {
    struct System {};
    world.RegisterSystem<System>();
}

TEST_F(EcsTest, RegisterSystemTwice) {
    static unsigned register_counter = 0;
    struct System {
        static void OnCreate(World&) { register_counter++; }
    };
    world.RegisterSystem<System>();
    world.RegisterSystem<System>();
    EXPECT_EQ(register_counter, 1);
}

TEST_F(EcsTest, Unregister) {
    static bool is_unregistered = false;
    struct System {
        static void OnDestroy(World&) { is_unregistered = true; }
    };

    world.RegisterSystem<System>();
    sm.Unregister<System>();
    EXPECT_TRUE(is_unregistered);
}

TEST_F(EcsTest, UnregisterSystemTwice) {
    static unsigned unregister_counter = 0;
    struct System {
        static void OnDestroy(World&) { unregister_counter++; }
    };

    world.RegisterSystem<System>();
    sm.Unregister<System>();
    sm.Unregister<System>();
    EXPECT_EQ(unregister_counter, 1);
}

TEST_F(EcsTest, AutoUnregisterSystemAfterWorldDestroyed) {
    static bool is_unregistered = false;
    struct System {
        static void OnDestroy(World&) { is_unregistered = true; }
    };

    {
        World _world("AutoUnregisterSystemAfterWorldDestroyed");
        _world.RegisterSystem<System>();
    }

    EXPECT_TRUE(is_unregistered);
}

TEST_F(EcsTest, SystemUpdate) {
    em.RegisterDynamicComponent({
        .name                = "DynamicComponent",
        .size                = sizeof(int),
        .default_constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; },
        .destructor          = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; },
    });

    static std::vector<Entity> invoked_entities;

    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request(
                "ImplicitFilterAll", [&](Entity e, LastFrame<Component_1> c1, Component_2& c2, std::byte* c3) {
                    invoked_entities.emplace_back(e);
                    c2.value                    = 200;
                    *reinterpret_cast<int*>(c3) = 300;
                },
                {"DynamicComponent"});
        }
    };

    auto entity_1 = em.Create();
    entity_1.Emplace<Component_1>();

    auto entity_2 = em.Create();
    entity_2.Emplace<Component_1>();
    entity_2.Emplace<Component_2>();

    auto entities_with_both = em.CreateMany<Component_1, Component_2>(100, {"DynamicComponent"});

    world.RegisterSystem<System>();
    world.Update(job_system);

    ASSERT_EQ(invoked_entities.size(), entities_with_both.size());
    for (const auto& [invoked_entity, entity] : std::ranges::views::zip(invoked_entities, entities_with_both)) {
        EXPECT_EQ(invoked_entity, entity);
        EXPECT_COMPONENT_EQ(entity, Component_1, 1);
        EXPECT_COMPONENT_EQ(entity, Component_2, 200);
        EXPECT_DYNAMIC_COMPONENT_EQ(entity, "DynamicComponent", 300);
    }
}

TEST_F(EcsTest, SystemUpdateWithNoEntities) {
    em.RegisterDynamicComponent({
        .name                = "DynamicComponent",
        .size                = sizeof(int),
        .default_constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; },
        .destructor          = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; },
    });

    static bool invoked = false;

    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request(
                "ImplicitFilterAll", [&](Entity, LastFrame<Component_1>, Component_2&, std::byte*) {
                    invoked = true;
                },
                {"DynamicComponent"});
        }
    };

    world.RegisterSystem<System>();
    world.Update(job_system);
    EXPECT_FALSE(invoked);
}

TEST_F(EcsTest, SystemUpdateOrder) {
    static std::vector<std::size_t> order;
    order.clear();
    const std::vector<std::size_t> expected_order{1, 2, 3, 4};

    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule
                .Request(
                    "Fn2",
                    [&](Component_1&) {
                        order.emplace_back(2);
                    })
                .Request(
                    "Fn4",
                    [&](const Component_1&) {
                        order.emplace_back(4);
                    })
                .Request(
                    "Fn3",
                    [&](Component_1&) {
                        order.emplace_back(3);
                    })
                .Request(
                    "Fn1",
                    [&](LastFrame<Component_1>) {
                        order.emplace_back(1);
                    });
        }
    };

    em.Create().Emplace<Component_1>();
    world.RegisterSystem<System>();
    world.Update(job_system);

    EXPECT_EQ(order.size(), 4);
    EXPECT_EQ(order, expected_order)
        << "The execution order of a request component is following"
        << "1(ReadBeforWrite). Execute parallel all function request with LastFrame<Component>"
        << "2(Write).  Execute all function request with Component sequentially in the order of requesting"
        << "3(ReadAfterWrite). Execute parallel all function request with const Component&";

    // The serial path must honour the same dependency order.
    order.clear();
    world.Update();
    EXPECT_EQ(order, expected_order);
}

TEST_F(EcsTest, SystemUpdateInCustomOrder) {
    static std::vector<std::size_t> order;
    order.clear();
    const std::vector<std::size_t> expected_order{2, 1};

    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.SetOrder("Fn2", "Fn1");
            schedule
                .Request("Fn1", [&](Component_1) {
                    order.emplace_back(1);
                })
                .Request("Fn2", [&](Component_1) {
                    order.emplace_back(2);
                });
        }
    };

    em.Create().Emplace<Component_1>();
    world.RegisterSystem<System>();
    world.Update(job_system);

    EXPECT_EQ(order, expected_order);

    order.clear();
    world.Update();
    EXPECT_EQ(order, expected_order);
}

TEST_F(EcsTest, SystemFilterAll) {
    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request(
                "FilterAll",
                [&](Entity entity) {
                    entity.Get<Component_1>().value = 10;
                    entity.Get<Component_2>().value = 20;
                },
                {},
                filter::All<Component_1, Component_2>());
        }
    };

    auto entity_with_one = em.Create();
    entity_with_one.Emplace<Component_1>();

    auto entity_with_both = em.Create();
    entity_with_both.Emplace<Component_1>();
    entity_with_both.Emplace<Component_2>();

    world.RegisterSystem<System>();
    world.Update(job_system);

    EXPECT_COMPONENT_EQ(entity_with_one, Component_1, 1) << "Component_1 should not be updated";

    EXPECT_COMPONENT_EQ(entity_with_both, Component_1, 10) << "Component_1 should be updated";
    EXPECT_COMPONENT_EQ(entity_with_both, Component_2, 20) << "Component_2 should be updated";
}

TEST_F(EcsTest, SystemFilterAny) {
    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request(
                "FilterAny",
                [&](Entity entity) {
                    if (entity.Has<Component_1>())
                        entity.Get<Component_1>().value = 100;

                    if (entity.Has<Component_2>())
                        entity.Get<Component_2>().value = 200;
                },
                {},
                filter::Any<Component_1, Component_2>());
        }
    };

    auto entity_with_first = em.Create();
    entity_with_first.Emplace<Component_1>();

    auto entity_with_second = em.Create();
    entity_with_second.Emplace<Component_2>();

    auto entity_with_third = em.Create();
    entity_with_third.Emplace<Component_3>();

    world.RegisterSystem<System>();
    world.Update(job_system);

    EXPECT_COMPONENT_EQ(entity_with_first, Component_1, 100) << "Component_1 should be updated";
    EXPECT_COMPONENT_EQ(entity_with_second, Component_2, 200) << "Component_2 should be updated";
    EXPECT_COMPONENT_EQ(entity_with_third, Component_3, 3) << "Component_3 should not be updated";
}

TEST_F(EcsTest, SystemFilterNone) {
    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request(
                "FilterNone",
                [](Component_1& c1) {
                    c1.value = 100;
                },
                {},
                filter::None<Component_2>());
        }
    };

    auto entity_with_one = em.Create();
    entity_with_one.Emplace<Component_1>();

    auto entity_with_both = em.Create();
    entity_with_both.Emplace<Component_1>();
    entity_with_both.Emplace<Component_2>();

    world.RegisterSystem<System>();
    world.Update(job_system);

    EXPECT_COMPONENT_EQ(entity_with_one, Component_1, 100) << "Component_1 should be updated";
    EXPECT_COMPONENT_EQ(entity_with_both, Component_1, 1) << "Component_1 should not be updated";
}
