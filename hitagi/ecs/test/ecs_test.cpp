#include <hitagi/math/transform.hpp>
#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/utils/test.hpp>

#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

using namespace hitagi::ecs;
using namespace hitagi::math;

template <Component T, typename V>
auto component_value_eq(const char* expr_entity, const char* expr_value, Entity entity, const V& value) -> ::testing::AssertionResult {
    static_assert(std::is_same_v<decltype(T::value), V>);

    if (auto component_value = entity.GetComponent<T>().value; component_value == value) {
        return ::testing::AssertionSuccess();
    } else {
        return ::testing::AssertionFailure() << fmt::format("Expect component value of {} is {}, but actual is {}", expr_entity, value, component_value);
    }
}

#define EXPECT_COMPONENT_EQ(_entity, _component, _value) \
    EXPECT_PRED_FORMAT2(component_value_eq<_component>, _entity, _value)

auto dynamic_component_value_eq(const char*      expr_entity,
                                const char*      expr_dynamic_component,
                                const char*      expr_value,
                                Entity           entity,
                                std::string_view dynamic_component,
                                int              value) -> ::testing::AssertionResult {
    auto component = entity.GetComponent(dynamic_component);
    if (component) {
        if (auto component_value = *reinterpret_cast<int*>(component); component_value == value) {
            return ::testing::AssertionSuccess();
        } else {
            return ::testing::AssertionFailure() << fmt::format("Expect component value of entity({}) is {}, but actual is {}", expr_entity, value, component_value);
        }
    } else {
        return testing::AssertionFailure() << fmt::format("The Entity({}) does not have component({})\n", expr_entity, expr_dynamic_component);
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

class EcsTest : public ::testing::Test {
public:
    EcsTest()
        : world(::testing::UnitTest::GetInstance()->current_test_info()->name()),
          em(world.GetEntityManager()),
          sm(world.GetSystemManager()) {}
    World          world;
    EntityManager& em;
    SystemManager& sm;
};

TEST_F(EcsTest, CreateStaticEntity) {
    auto entity_1 = em.Create<Component_1>();
    EXPECT_TRUE(entity_1);
    auto entity_2 = em.Create<Component_1, Component_2>();
    EXPECT_TRUE(entity_2);
    EXPECT_NE(entity_1, entity_2);
}

TEST_F(EcsTest, CreateAndDestroyDynamicComponent) {
    bool is_constructed = false;
    bool is_destructed  = false;

    std::byte* constructor_p_data = nullptr;
    std::byte* destructor_p_data  = nullptr;

    em.RegisterDynamicComponent({
        .name        = "DynamicComponent",
        .size        = sizeof(int),
        .constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; constructor_p_data = data; is_constructed=true; },
        .destructor  = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; destructor_p_data = data; is_destructed=true; },
    });

    auto entity = em.Create({"DynamicComponent"});
    EXPECT_TRUE(entity);
    EXPECT_TRUE(is_constructed)
        << "DynamicComponent should be constructed after entity created";

    EXPECT_DYNAMIC_COMPONENT_EQ(entity, "DynamicComponent", 1);
    std::byte* p_data = entity.GetComponent("DynamicComponent");
    EXPECT_EQ(p_data, constructor_p_data) << "GetDynamicComponent should return the same pointer as constructor";

    em.Destroy(entity);
    EXPECT_FALSE(entity);
    EXPECT_TRUE(is_destructed)
        << "DynamicComponent should be destructed after entity destroyed";

    EXPECT_EQ(p_data, destructor_p_data)
        << "GetDynamicComponent should return the same pointer as destructor";
}

TEST_F(EcsTest, CreateManyEntities) {
    auto entities = em.CreateMany<Component_1>(10);
    EXPECT_EQ(entities.size(), 10);
    for (const auto& entity : entities) {
        EXPECT_TRUE(entity);
    }
}

TEST_F(EcsTest, DestructComponentAfterWorldDestroyed) {
    bool is_destructed = false;
    {
        World _world("DestructComponentAfterWorldDestroyed");
        _world.GetEntityManager().RegisterDynamicComponent({
            .name       = "DynamicComponent",
            .size       = sizeof(int),
            .destructor = [&](std::byte* data) { is_destructed = true; },
        });
        _world.GetEntityManager().Create<Component_1>({"DynamicComponent"});
    }
    EXPECT_TRUE(is_destructed);
}

TEST_F(EcsTest, CreateZeroEntities) {
    auto entities = em.CreateMany<Component_1>(0);
    EXPECT_TRUE(entities.empty());
}

TEST_F(EcsTest, CreateEntityWithoutComponent) {
    auto entity = em.Create();
    EXPECT_TRUE(entity);
    EXPECT_TRUE(entity);
}

TEST_F(EcsTest, DestroyEntity) {
    auto entity = em.Create<Component_1>();
    EXPECT_TRUE(entity);
    EXPECT_EQ(em.NumEntities(), 1);
    em.Destroy(entity);
    EXPECT_EQ(em.NumEntities(), 0);
    EXPECT_FALSE(entity);
}

TEST_F(EcsTest, DestroyNonExistentEntity) {
    Entity invalid_entity{};
    EXPECT_THROW(em.Destroy(invalid_entity), std::out_of_range);
}

TEST_F(EcsTest, DestroyRepeatedly) {
    auto entity = em.Create<Component_1>();
    EXPECT_TRUE(entity);
    EXPECT_EQ(em.NumEntities(), 1);
    em.Destroy(entity);
    EXPECT_EQ(em.NumEntities(), 0);
    EXPECT_FALSE(entity);
    EXPECT_THROW(em.Destroy(entity), std::out_of_range);
}

TEST_F(EcsTest, AccessComponent) {
    auto entity = em.Create<Component_1, Component_2>();

    ASSERT_TRUE(entity.HasComponent<Entity>()) << "any entity should always have Entity component";
    EXPECT_EQ(entity.GetComponent<Entity>(), entity) << "Entity component should always be the entity itself";
    EXPECT_COMPONENT_EQ(entity, Component_1, 1);
    EXPECT_COMPONENT_EQ(entity, Component_2, 2);
}

TEST_F(EcsTest, GetComponentThatDoesNotExist) {
    auto entity = em.Create<Component_1>();
    EXPECT_FALSE(entity.HasComponent<Component_2>());
}

TEST_F(EcsTest, ModifyEntity) {
    auto entity = em.Create<Component_1>();
    {
        auto& component = entity.GetComponent<Component_1>();
        component.value = 2;
    }
    EXPECT_COMPONENT_EQ(entity, Component_1, 2);
}

TEST_F(EcsTest, AttachComponent) {
    em.RegisterDynamicComponent({
        .name        = "DynamicComponent",
        .size        = sizeof(int),
        .constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; },
        .destructor  = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; },
    });

    auto entity = em.Create<Component_1>();
    entity.Attach<Component_2, Component_3>({"DynamicComponent"});
    EXPECT_COMPONENT_EQ(entity, Component_1, 1);
    EXPECT_COMPONENT_EQ(entity, Component_2, 2);
    EXPECT_COMPONENT_EQ(entity, Component_3, 3);
    EXPECT_DYNAMIC_COMPONENT_EQ(entity, "DynamicComponent", 1);
}

TEST_F(EcsTest, AttachComponentThatAlreadyExists) {
    auto entity = em.Create<Component_1>();

    entity.GetComponent<Component_1>().value = 3;
    EXPECT_THROW(entity.Attach<Component_1>(), std::invalid_argument);
}

TEST_F(EcsTest, DetachComponent) {
    em.RegisterDynamicComponent({
        .name        = "DynamicComponent",
        .size        = sizeof(int),
        .constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; },
        .destructor  = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; },
    });

    auto entity = em.Create<Component_1, Component_2, Component_3>({"DynamicComponent"});
    entity.Detach<Component_2, Component_3>({"DynamicComponent"});
    EXPECT_COMPONENT_EQ(entity, Component_1, 1);
    EXPECT_FALSE(entity.HasComponent<Component_2>());
    EXPECT_FALSE(entity.HasComponent<Component_3>());
    EXPECT_FALSE(entity.HasComponent("DynamicComponent"));
}

TEST_F(EcsTest, DetachComponentThatDoesNotExist) {
    auto entity = em.Create<Component_1>();
    EXPECT_THROW(entity.Detach<Component_2>(), std::invalid_argument);
}

TEST_F(EcsTest, DestructComponentAfterDetach) {
    bool is_destructed = false;
    em.RegisterDynamicComponent({
        .name        = "DynamicComponent",
        .size        = sizeof(int),
        .constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; },
        .destructor  = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; is_destructed = true; },
    });
    auto entity = em.Create<Component_1>({"DynamicComponent"});
    entity.Detach({"DynamicComponent"});
    EXPECT_TRUE(is_destructed);
}

TEST_F(EcsTest, Register) {
    struct System {};
    sm.Register<System>();
}

TEST_F(EcsTest, RegisterSystemTwice) {
    static unsigned register_counter = 0;
    struct System {
        static void OnCreate(World& world) { register_counter++; }
    };
    sm.Register<System>();
    sm.Register<System>();
    EXPECT_EQ(register_counter, 1);
}

TEST_F(EcsTest, Unregister) {
    static bool is_unregistered = false;
    struct System {
        static void OnDestroy(World& world) { is_unregistered = true; }
    };

    sm.Register<System>();
    sm.Unregister<System>();
    EXPECT_TRUE(is_unregistered);
}

TEST_F(EcsTest, UnregisterSystemTwice) {
    static unsigned unregister_counter = 0;
    struct System {
        static void OnDestroy(World& world) { unregister_counter++; }
    };

    sm.Register<System>();
    sm.Unregister<System>();
    sm.Unregister<System>();
    EXPECT_EQ(unregister_counter, 1);
}

TEST_F(EcsTest, AutoUnregisterSystemAfterWorldDestroyed) {
    static bool is_unregistered = false;
    struct System {
        static void OnDestroy(World& world) { is_unregistered = true; }
    };

    {
        World _world("AutoUnregisterSystemAfterWorldDestroyed");
        _world.GetSystemManager().Register<System>();
    }

    EXPECT_TRUE(is_unregistered);
}

TEST_F(EcsTest, SystemUpdate) {
    em.RegisterDynamicComponent({
        .name        = "DynamicComponent",
        .size        = sizeof(int),
        .constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; },
        .destructor  = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; },
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

    em.Create<Component_1>();
    em.Create<Component_1, Component_2>();
    auto entities_with_both = em.CreateMany<Component_1, Component_2>(100, {"DynamicComponent"});

    sm.Register<System>();
    world.Update();

    ASSERT_EQ(invoked_entities.size(), entities_with_both.size());
    for (auto [invoked_entity, entity] : ranges::views::zip(invoked_entities, entities_with_both)) {
        EXPECT_EQ(invoked_entity, entity);
        EXPECT_COMPONENT_EQ(entity, Component_1, 1);
        EXPECT_COMPONENT_EQ(entity, Component_2, 200);
        EXPECT_DYNAMIC_COMPONENT_EQ(entity, "DynamicComponent", 300);
    }
}

TEST_F(EcsTest, SystemUpdateWithNoEntities) {
    em.RegisterDynamicComponent({
        .name        = "DynamicComponent",
        .size        = sizeof(int),
        .constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; },
        .destructor  = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; },
    });

    static bool invoked = false;

    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule.Request(
                "ImplicitFilterAll", [&](Entity e, LastFrame<Component_1> c1, Component_2& c2, std::byte* c3) {
                    invoked = true;
                },
                {"DynamicComponent"});
        }
    };

    sm.Register<System>();
    world.Update();
    EXPECT_FALSE(invoked);
}

TEST_F(EcsTest, SystemUpdateOrder) {
    static std::pmr::vector<std::size_t> order;
    std::pmr::vector<std::size_t>        expected_order{1, 2, 3, 4};

    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule
                .Request(
                    "Fn2",
                    [&](Component_1& c1) {
                        order.emplace_back(2);
                    })
                .Request(
                    "Fn4",
                    [&](const Component_1& c1) {
                        order.emplace_back(4);
                    })
                .Request(
                    "Fn3",
                    [&](Component_1& c1) {
                        order.emplace_back(3);
                    })
                .Request(
                    "Fn1",
                    [&](LastFrame<Component_1> c1) {
                        order.emplace_back(1);
                    });
        }
    };

    em.Create<Component_1>();
    sm.Register<System>();
    world.Update();

    EXPECT_EQ(order.size(), 4);
    EXPECT_EQ(order, expected_order)
        << "The execution order of a request component is following"
        << "1(ReadBeforWrite). Execute parallel all function request with LastFrame<Component>"
        << "2(Write).  Execute all function request with Component sequentially in the order of requesting"
        << "3(ReadAfterWrite). Execute parallel all function request with const Component&";
}

TEST_F(EcsTest, SystemUpdateInCustomOrder) {
    static std::pmr::vector<std::size_t> order;
    std::pmr::vector<std::size_t>        expected_order{2, 1};

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

    em.Create<Component_1>();
    sm.Register<System>();
    world.Update();

    EXPECT_EQ(order, expected_order);
}

TEST_F(EcsTest, SystemFilterAll) {
    struct System {
        static void OnUpdate(Schedule& schedule) {
            auto& em = schedule.world.GetEntityManager();
            schedule.Request(
                "FilterAll",
                [&](Entity entity) {
                    entity.GetComponent<Component_1>().value = 10;
                    entity.GetComponent<Component_2>().value = 20;
                },
                {},
                filter::All<Component_1, Component_2>());
        }
    };

    auto entity_with_one  = em.Create<Component_1>();
    auto entity_with_both = em.Create<Component_1, Component_2>();

    sm.Register<System>();
    world.Update();

    EXPECT_COMPONENT_EQ(entity_with_one, Component_1, 1) << "Component_1 should not be updated";

    EXPECT_COMPONENT_EQ(entity_with_both, Component_1, 10) << "Component_1 should be updated";
    EXPECT_COMPONENT_EQ(entity_with_both, Component_2, 20) << "Component_2 should be updated";
}

TEST_F(EcsTest, SystemFilterAny) {
    struct System {
        static void OnUpdate(Schedule& schedule) {
            auto& em = schedule.world.GetEntityManager();

            schedule.Request(
                "FilterAny",
                [&](Entity entity) {
                    if (entity.HasComponent<Component_1>())
                        entity.GetComponent<Component_1>().value = 100;

                    if (entity.HasComponent<Component_2>())
                        entity.GetComponent<Component_2>().value = 200;
                },
                {},
                filter::Any<Component_1, Component_2>());
        }
    };

    auto entity_with_first  = em.Create<Component_1>();
    auto entity_with_second = em.Create<Component_2>();
    auto entity_with_third  = em.Create<Component_3>();

    sm.Register<System>();
    world.Update();

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

    auto entity_with_one  = em.Create<Component_1>();
    auto entity_with_both = em.Create<Component_1, Component_2>();

    sm.Register<System>();
    world.Update();

    EXPECT_COMPONENT_EQ(entity_with_one, Component_1, 100) << "Component_1 should be updated";
    EXPECT_COMPONENT_EQ(entity_with_both, Component_1, 1) << "Component_1 should not be updated";
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}