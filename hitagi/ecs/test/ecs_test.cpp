#include <hitagi/math/transform.hpp>
#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi::ecs;
using namespace hitagi::math;

template <Component T, typename V>
auto component_value_eq(const char* expr_world, const char* expr_entity, const char* expr_value, const World& world, Entity entity, const V& value) -> ::testing::AssertionResult {
    static_assert(std::is_same_v<decltype(T::value), V>);

    auto component = world.GetEntityManager().GetComponent<T>(entity);
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

class EcsTest : public ::testing::Test {
public:
    EcsTest() : world(::testing::UnitTest::GetInstance()->current_test_info()->name()) {}
    World world;
};

TEST_F(EcsTest, CreateEntity) {
    struct Component_1 {
        int value;
    };
    struct Component_2 {
        int value;
    };

    auto entity_1 = world.GetEntityManager().Create<Component_1>();
    EXPECT_TRUE(entity_1);
    EXPECT_TRUE(world.GetEntityManager().Has(entity_1));
    auto entity_2 = world.GetEntityManager().Create<Component_1, Component_2>();
    EXPECT_TRUE(entity_2);
    EXPECT_TRUE(world.GetEntityManager().Has(entity_2));
    EXPECT_NE(entity_1, entity_2);

    DynamicComponent dynamic_component{
        .name = "DynamicComponent",
        .size = 8,
    };
    auto dynamic_entity = world.GetEntityManager().Create({dynamic_component});
    EXPECT_TRUE(dynamic_entity);
    EXPECT_TRUE(world.GetEntityManager().Has(dynamic_entity));
    EXPECT_NE(dynamic_entity, entity_1);
    EXPECT_NE(dynamic_entity, entity_2);

    auto entities = world.GetEntityManager().CreateMany<Component_1, Component_2>(100, {dynamic_component});
    EXPECT_EQ(entities.size(), 100);
    for (auto entity : entities) {
        EXPECT_TRUE(entity);
        EXPECT_TRUE(world.GetEntityManager().Has(entity));
    }

    EXPECT_EQ(world.GetEntityManager().NumEntities(), 103);
}

TEST_F(EcsTest, DestroyEntity) {
    struct Component_1 {
        int value;
    };
    auto entity = world.GetEntityManager().Create<Component_1>();
    EXPECT_EQ(world.GetEntityManager().NumEntities(), 1);
    world.GetEntityManager().Destroy(entity);
    EXPECT_EQ(world.GetEntityManager().NumEntities(), 0);
}

TEST_F(EcsTest, DifferentComponents) {
    struct Component_1 {
        vec3f value;
    };
    struct Component_2 {
        vec2f value;
    };
    DynamicComponent dynamic_component_1{
        .name = "DynamicComponent_1",
        .size = 8,
    };
    DynamicComponent dynamic_component_2{
        .name = "DynamicComponent_2",
        .size = 8,
    };
    auto id1 = detail::get_archetype_id<Component_1>({dynamic_component_1});
    auto id2 = detail::get_archetype_id<Component_2>({dynamic_component_2});
    EXPECT_NE(id1, id2);
}

TEST_F(EcsTest, DifferentOrderComponents) {
    struct Component_1 {
        vec3f value;
    };
    struct Component_2 {
        vec2f value;
    };
    struct Transform {
        mat4f value;
    };
    DynamicComponent dynamic_component_1{
        .name = "DynamicComponent_1",
        .size = 8,
    };
    DynamicComponent dynamic_component_2{
        .name = "DynamicComponent_2",
        .size = 8,
    };

    auto id1 = detail::get_archetype_id<Component_1, Component_2, Transform>({dynamic_component_1, dynamic_component_2});
    auto id2 = detail::get_archetype_id<Transform, Component_2, Component_1>({dynamic_component_2, dynamic_component_1});
    EXPECT_EQ(id1, id2);

    World world(::testing::UnitTest::GetInstance()->current_test_info()->name());
    world.GetEntityManager().Create<Component_1, Component_2, Transform>({dynamic_component_1, dynamic_component_2});
    world.GetEntityManager().Create<Transform, Component_2, Component_1>({dynamic_component_2, dynamic_component_1});
    auto archetype = world.GetEntityManager().GetArchetype(Filter{}.All<Component_1, Component_2, Transform>());
    EXPECT_FALSE(archetype.empty());
    if (!archetype.empty()) {
        EXPECT_EQ(archetype.front()->NumEntities(), 2);
    }
}

TEST_F(EcsTest, AccessComponent) {
    struct Component_1 {
        int value = 1;
    };
    struct Component_2 {
        int value = 2;
    };
    auto entity = world.GetEntityManager().Create<Component_1, Component_2>();

    EXPECT_PRED_FORMAT3(component_value_eq<Component_1>, world, entity, 1);
    EXPECT_PRED_FORMAT3(component_value_eq<Component_2>, world, entity, 2);
}

TEST_F(EcsTest, ModifyEntity) {
    struct Component_1 {
        int value = 1;
    };

    auto entity = world.GetEntityManager().Create<Component_1>();
    {
        auto component = world.GetEntityManager().GetComponent<Component_1>(entity);
        EXPECT_TRUE(component.has_value());
        if (component.has_value())
            component->get().value = 2;
    }
    EXPECT_PRED_FORMAT3(component_value_eq<Component_1>, world, entity, 2);
}

TEST_F(EcsTest, RegisterSystem) {
    struct Component_1 {
        int value = 1;
    };
    struct Component_2 {
        int value = 1;
    };

    struct System1 {
        static void OnUpdate(Schedule& schedule) {
            schedule
                .Request(
                    "Update Component_1",
                    [](Component_1& Component_1) {
                        Component_1.value++;
                    })
                .Request(
                    "Update Component_2",
                    [](Component_2& Component_2) {
                        Component_2.value = 3;
                    });
        }
    };

    auto entity = world.GetEntityManager().Create<Component_1, Component_2>();
    world.RegisterSystem<System1>("System1");
    world.Update();

    EXPECT_PRED_FORMAT3(component_value_eq<Component_1>, world, entity, 2);
    EXPECT_PRED_FORMAT3(component_value_eq<Component_2>, world, entity, 3);
}

TEST_F(EcsTest, FilterTest) {
    World world(::testing::UnitTest::GetInstance()->current_test_info()->name());

    struct Component_1 {
        int value = 0;
    };
    struct Component_2 {
        int value = 0;
    };
    struct Component_3 {
        int value = 0;
    };
    struct Component_4 {
        int value = 0;
    };
    struct Component_5 {
        int value = 0;
    };
    struct Component_6 {
        int value = 0;
    };
    struct System {
        static void OnUpdate(Schedule& schedule) {
            schedule
                .Request(
                    "AllFilter", [&](Entity entity, Component_1& a) {
                        a.value = 1;
                        if (auto component_2 = schedule.world.GetEntityManager().GetComponent<Component_2>(entity);
                            component_2.has_value()) {
                            component_2->get().value = 2;
                        }
                        if (auto component_3 = schedule.world.GetEntityManager().GetComponent<Component_3>(entity);
                            component_3.has_value()) {
                            component_3->get().value = 2;
                        }
                    },
                    Filter().All<Component_2, Component_3>())
                .Request(
                    "AnyFilter", [&](Entity entity) {
                        if (auto component_3 = schedule.world.GetEntityManager().GetComponent<Component_3>(entity);
                            component_3.has_value()) {
                            component_3->get().value = 3;
                        }
                        if (auto component_4 = schedule.world.GetEntityManager().GetComponent<Component_4>(entity);
                            component_4.has_value()) {
                            component_4->get().value = 4;
                        }
                    },
                    Filter().Any<Component_3, Component_4>())
                .Request(
                    "NoneFilter", [&](Component_5& a, Component_6& b) {
                        a.value = 5;
                        b.value = 6;
                    },
                    Filter().None<Component_1>());
        }
    };
    world.RegisterSystem<System>("FilterTestSystem");

    auto entity_1 = world.GetEntityManager().Create<Component_1, Component_2, Component_3>();
    auto entity_2 = world.GetEntityManager().Create<Component_1, Component_3>();
    auto entity_3 = world.GetEntityManager().Create<Component_2, Component_4>();
    auto entity_4 = world.GetEntityManager().Create<Component_1, Component_5, Component_6>();

    world.Update();
    {
        SCOPED_TRACE("Filter::All");
        EXPECT_PRED_FORMAT3(component_value_eq<Component_1>, world, entity_1, 1);
        EXPECT_PRED_FORMAT3(component_value_eq<Component_2>, world, entity_1, 2);
    }

    {
        SCOPED_TRACE("Filter::Any");
        EXPECT_PRED_FORMAT3(component_value_eq<Component_1>, world, entity_2, 0);
        EXPECT_PRED_FORMAT3(component_value_eq<Component_3>, world, entity_2, 3);
        EXPECT_PRED_FORMAT3(component_value_eq<Component_2>, world, entity_3, 0);
        EXPECT_PRED_FORMAT3(component_value_eq<Component_4>, world, entity_3, 4);
    }

    {
        SCOPED_TRACE("Filter::None");
        EXPECT_PRED_FORMAT3(component_value_eq<Component_5>, world, entity_4, 0);
        EXPECT_PRED_FORMAT3(component_value_eq<Component_6>, world, entity_4, 0);
    }
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::err);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}