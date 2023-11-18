#include <hitagi/math/transform.hpp>
#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/utils/test.hpp>

#include <spdlog/spdlog.h>

using namespace hitagi::ecs;
using namespace hitagi::math;

template <Component T, typename V>
auto component_value_eq(const char* expr_entity_manager, const char* expr_entity, const char* expr_value, const EntityManager& entity_manager, Entity entity, const V& value) -> ::testing::AssertionResult {
    static_assert(std::is_same_v<decltype(T::value), V>);

    auto component = entity_manager.GetComponent<T>(entity);
    if (component.has_value()) {
        if (auto component_value = component->get().value; component_value == value) {
            return ::testing::AssertionSuccess();
        } else {
            return ::testing::AssertionFailure() << fmt::format("Expect component value of entity({}) is {}, but actual is {}", expr_entity, value, component_value);
        }
    } else {
        return testing::AssertionFailure() << fmt::format("The Entity({}) does not have component\n", expr_entity);
    }
}

#define EXPECT_COMPONENT_EQ(_entity_manager, _entity, _component, _value) \
    EXPECT_PRED_FORMAT3(component_value_eq<_component>, _entity_manager, _entity, _value)

auto dynamic_component_value_eq(const char*             expr_entity_manager,
                                const char*             expr_entity,
                                const char*             expr_dynamic_component,
                                const char*             expr_value,
                                const EntityManager&    entity_manager,
                                Entity                  entity,
                                const DynamicComponent& dynamic_component,
                                int                     value) -> ::testing::AssertionResult {
    auto component = entity_manager.GetDynamicComponent(entity, dynamic_component);
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

#define EXPECT_DYNAMIC_COMPONENT_EQ(_entity_manager, _entity, _dynamic_component, _value) \
    EXPECT_PRED_FORMAT4(dynamic_component_value_eq, _entity_manager, _entity, _dynamic_component, _value)

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
    EcsTest() : world(::testing::UnitTest::GetInstance()->current_test_info()->name()), em(world.GetEntityManager()) {}
    World          world;
    EntityManager& em;
};

TEST_F(EcsTest, CreateTypedEntity) {
    auto entity_1 = em.Create<Component_1>();
    EXPECT_TRUE(entity_1);
    EXPECT_TRUE(em.Has(entity_1));
    auto entity_2 = em.Create<Component_1, Component_2>();
    EXPECT_TRUE(entity_2);
    EXPECT_TRUE(em.Has(entity_2));
    EXPECT_NE(entity_1, entity_2);
}

TEST_F(EcsTest, CreateAndDestroyDynamicComponent) {
    bool             is_constructed = false;
    bool             is_destructed  = false;
    DynamicComponent dynamic_component{
        .name        = "DynamicComponent",
        .size        = sizeof(int),
        .constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; is_constructed=true; },
        .destructor  = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; is_destructed=true; },
    };
    auto entity = em.Create({dynamic_component});
    EXPECT_TRUE(em.Has(entity));
    EXPECT_TRUE(is_constructed);

    EXPECT_DYNAMIC_COMPONENT_EQ(em, entity, dynamic_component, 1);

    em.Destroy(entity);
    EXPECT_FALSE(em.Has(entity));
    EXPECT_TRUE(is_destructed);
}

TEST_F(EcsTest, CreateZeroEntities) {
    auto entities = em.CreateMany<Component_1>(0);
    EXPECT_TRUE(entities.empty());
}

TEST_F(EcsTest, CreateEntityWithoutComponent) {
    auto entity = em.Create();
    EXPECT_TRUE(entity);
    EXPECT_TRUE(em.Has(entity));
}

TEST_F(EcsTest, DestroyNonExistentEntity) {
    Entity invalid_entity{12345};
    EXPECT_THROW(em.Destroy(invalid_entity), std::out_of_range);
}

TEST_F(EcsTest, DestroyEntity) {
    auto entity = em.Create<Component_1>();
    EXPECT_EQ(em.NumEntities(), 1);
    em.Destroy(entity);
    EXPECT_EQ(em.NumEntities(), 0);
}

TEST_F(EcsTest, AccessComponent) {
    auto entity = em.Create<Component_1, Component_2>();

    ASSERT_TRUE(em.GetComponent<Entity>(entity).has_value()) << "any entity should always have Entity component";
    EXPECT_EQ(em.GetComponent<Entity>(entity)->get(), entity) << "Entity component should always be the entity itself";
    EXPECT_COMPONENT_EQ(em, entity, Component_1, 1);
    EXPECT_COMPONENT_EQ(em, entity, Component_2, 2);
}

TEST_F(EcsTest, GetComponentThatDoesNotExist) {
    auto entity    = em.Create<Component_1>();
    auto component = em.GetComponent<Component_2>(entity);
    EXPECT_FALSE(component.has_value());
}

TEST_F(EcsTest, ModifyEntity) {
    auto entity = em.Create<Component_1>();
    {
        auto component = em.GetComponent<Component_1>(entity);
        EXPECT_TRUE(component.has_value());
        if (component.has_value())
            component->get().value = 2;
    }
    EXPECT_COMPONENT_EQ(em, entity, Component_1, 2);
}

TEST_F(EcsTest, AttachComponent) {
    DynamicComponent dynamic_component{
        .name        = "DynamicComponent",
        .size        = sizeof(int),
        .constructor = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 1; },
        .destructor  = [&](std::byte* data) { *reinterpret_cast<int*>(data) = 0; },
    };

    auto entity = em.Create<Component_1>();
    em.Attach<Component_2, Component_3>(entity, {dynamic_component});
    EXPECT_COMPONENT_EQ(em, entity, Component_1, 1);
    EXPECT_COMPONENT_EQ(em, entity, Component_2, 2);
    EXPECT_COMPONENT_EQ(em, entity, Component_3, 3);
    EXPECT_DYNAMIC_COMPONENT_EQ(em, entity, dynamic_component, 1);
}

TEST_F(EcsTest, AttachComponentThatAlreadyExists) {
    auto entity = em.Create<Component_1>();

    em.GetComponent<Component_1>(entity)->get().value = 3;
    EXPECT_THROW(em.Attach<Component_1>(entity), std::invalid_argument);
}

TEST_F(EcsTest, DetachComponent) {
    auto entity = em.Create<Component_1, Component_2, Component_3>();
    em.Detach<Component_2, Component_3>(entity);
    EXPECT_COMPONENT_EQ(em, entity, Component_1, 1);
    EXPECT_FALSE(em.GetComponent<Component_2>(entity).has_value());
    EXPECT_FALSE(em.GetComponent<Component_3>(entity).has_value());
}

TEST_F(EcsTest, DetachComponentThatDoesNotExist) {
    auto entity = em.Create<Component_1>();
    EXPECT_THROW(em.Detach<Component_2>(entity), std::invalid_argument);
}

TEST_F(EcsTest, RegisterSystem) {
    static bool is_registered = false;
    struct System {
        static void OnRegister(World& world) { is_registered = true; }
        static void OnUnregister(World& world) {}
        static void OnUpdate(Schedule& schedule) {}
    };
    world.RegisterSystem<System>();
    EXPECT_TRUE(is_registered);
}

TEST_F(EcsTest, RegisterSystemTwice) {
    static unsigned register_counter = 0;
    struct System {
        static void OnRegister(World& world) { register_counter++; }
        static void OnUnregister(World& world) {}
        static void OnUpdate(Schedule& schedule) {}
    };
    world.RegisterSystem<System>();
    world.RegisterSystem<System>();
    EXPECT_EQ(register_counter, 1);
}

TEST_F(EcsTest, UnregisterSystem) {
    static bool is_unregistered = false;
    struct System {
        static void OnRegister(World& world) {}
        static void OnUnregister(World& world) { is_unregistered = true; }
        static void OnUpdate(Schedule& schedule) {}
    };

    world.RegisterSystem<System>();
    world.UnregisterSystem<System>();
    EXPECT_TRUE(is_unregistered);
}

TEST_F(EcsTest, UnregisterSystemTwice) {
    static unsigned unregister_counter = 0;
    struct System {
        static void OnRegister(World& world) {}
        static void OnUnregister(World& world) { unregister_counter++; }
        static void OnUpdate(Schedule& schedule) {}
    };

    world.RegisterSystem<System>();
    world.UnregisterSystem<System>();
    world.UnregisterSystem<System>();
    EXPECT_EQ(unregister_counter, 1);
}

TEST_F(EcsTest, UpdateSystem) {
    static unsigned update_counter = 0;
    struct System {
        static void OnRegister(World& world) {}
        static void OnUnregister(World& world) {}
        static void OnUpdate(Schedule& schedule) { update_counter++; }
    };

    world.RegisterSystem<System>();
    world.Update();
    EXPECT_EQ(update_counter, 1);
    world.Update();
    EXPECT_EQ(update_counter, 2);
}

TEST_F(EcsTest, SystemImplicitFilterAll) {
    struct System {
        static void OnRegister(World& world) {}
        static void OnUnregister(World& world) {}

        static void OnUpdate(Schedule& schedule) {
            schedule.Request(
                "ImplicitFilterAll", [](Component_1& c1, Component_2& c2) {
                    c1.value = 10;
                    c2.value = 20;
                });
        }
    };

    auto entity_with_one  = em.Create<Component_1>();
    auto entity_with_both = em.Create<Component_1, Component_2>();

    world.RegisterSystem<System>();
    world.Update();

    EXPECT_COMPONENT_EQ(em, entity_with_one, Component_1, 1) << "Component_1 should not be updated";

    EXPECT_COMPONENT_EQ(em, entity_with_both, Component_1, 10) << "Component_1 should be updated";
    EXPECT_COMPONENT_EQ(em, entity_with_both, Component_2, 20) << "Component_2 should be updated";
}

TEST_F(EcsTest, SystemFilterAll) {
    struct System {
        static void OnRegister(World& world) {}
        static void OnUnregister(World& world) {}

        static void OnUpdate(Schedule& schedule) {
            auto& em = schedule.world.GetEntityManager();
            schedule.Request(
                "FilterAll", [&](Entity entity) {
                    if (auto component = em.GetComponent<Component_1>(entity); component.has_value())
                        component->get().value = 10;
                    if (auto component = em.GetComponent<Component_2>(entity); component.has_value())
                        component->get().value = 20;
                },
                Filter().All<Component_1, Component_2>());
        }
    };

    auto entity_with_one  = em.Create<Component_1>();
    auto entity_with_both = em.Create<Component_1, Component_2>();

    world.RegisterSystem<System>();
    world.Update();

    EXPECT_COMPONENT_EQ(em, entity_with_one, Component_1, 1) << "Component_1 should not be updated";

    EXPECT_COMPONENT_EQ(em, entity_with_both, Component_1, 10) << "Component_1 should be updated";
    EXPECT_COMPONENT_EQ(em, entity_with_both, Component_2, 20) << "Component_2 should be updated";
}

TEST_F(EcsTest, SystemFilterAny) {
    struct System {
        static void OnRegister(World& world) {}
        static void OnUnregister(World& world) {}

        static void OnUpdate(Schedule& schedule) {
            auto& em = schedule.world.GetEntityManager();

            schedule.Request(
                "FilterAny", [&](Entity entity) {
                    if (auto component = em.GetComponent<Component_1>(entity); component.has_value())
                        component->get().value = 100;
                    if (auto component = em.GetComponent<Component_2>(entity); component.has_value())
                        component->get().value = 200;
                },
                Filter().Any<Component_1, Component_2>());
        }
    };

    auto entity_with_first  = em.Create<Component_1>();
    auto entity_with_second = em.Create<Component_2>();
    auto entity_with_third  = em.Create<Component_3>();

    world.RegisterSystem<System>();
    world.Update();

    EXPECT_COMPONENT_EQ(em, entity_with_first, Component_1, 100) << "Component_1 should be updated";
    EXPECT_COMPONENT_EQ(em, entity_with_second, Component_2, 200) << "Component_2 should be updated";
    EXPECT_COMPONENT_EQ(em, entity_with_third, Component_3, 3) << "Component_3 should not be updated";
}

TEST_F(EcsTest, SystemFilterNone) {
    struct System {
        static void OnRegister(World& world) {}
        static void OnUnregister(World& world) {}

        static void OnUpdate(Schedule& schedule) {
            schedule.Request(
                "FilterNone", [](Component_1& c1) {
                    c1.value = 100;
                },
                Filter().None<Component_2>());
        }
    };

    auto entity_with_one  = em.Create<Component_1>();
    auto entity_with_both = em.Create<Component_1, Component_2>();

    world.RegisterSystem<System>();
    world.Update();

    EXPECT_COMPONENT_EQ(em, entity_with_one, Component_1, 100) << "Component_1 should be updated";
    EXPECT_COMPONENT_EQ(em, entity_with_both, Component_1, 1) << "Component_1 should not be updated";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}