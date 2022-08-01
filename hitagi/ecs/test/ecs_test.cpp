#include <hitagi/utils/test.hpp>
#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <vector>

#include <spdlog/spdlog.h>

using namespace hitagi::ecs;

TEST(EcsTest, CreateEntity) {
    World world("EcsTest.CreateEntity");

    struct Position {
        int value = 1;
    };
    struct Velocity {
        int value = 2;
    };

    {
        auto pos_entity = world.CreateEntity<Position>();
        EXPECT_TRUE(pos_entity);
        auto component = world.AccessEntity<Position>(pos_entity);
        EXPECT_TRUE(component.has_value());
        EXPECT_EQ(component->get().value, 1);
        world.DestoryEntity(pos_entity);
    }

    {
        auto pos_vel_entity = world.CreateEntity<Position, Velocity>();
        EXPECT_TRUE(pos_vel_entity);
        auto pos_component = world.AccessEntity<Position>(pos_vel_entity);
        EXPECT_TRUE(pos_component.has_value());
        EXPECT_EQ(pos_component->get().value, 1);

        auto vel_component = world.AccessEntity<Velocity>(pos_vel_entity);
        EXPECT_TRUE(vel_component.has_value());
        EXPECT_EQ(vel_component->get().value, 2);

        world.DestoryEntity(pos_vel_entity);
    }

    {
        Position pos;
        Velocity vel;
        auto     pos_vel_entity = world.CreateEntity(pos, vel);

        auto pos_component = world.AccessEntity<Position>(pos_vel_entity);
        EXPECT_TRUE(pos_component.has_value());
        EXPECT_EQ(pos_component->get().value, 1);

        auto vel_component = world.AccessEntity<Velocity>(pos_vel_entity);
        EXPECT_TRUE(vel_component.has_value());
        EXPECT_EQ(vel_component->get().value, 2);
        world.DestoryEntity(pos_vel_entity);
    }
}

TEST(EcsTest, SameID) {
    struct Position {
        hitagi::math::vec3f value;
    };
    struct Velocity {
        hitagi::math::vec2f value;
    };
    struct Transform {
        hitagi::math::mat4f value;
    };
    auto id1 = get_archetype_id<Position, Velocity, Transform>();
    auto id2 = get_archetype_id<Transform, Velocity, Position>();
    EXPECT_EQ(id1, id2);

    World world("EcsTest.SameID");
    world.CreateEntity<Position, Velocity, Transform>();
    world.CreateEntity<Transform, Velocity, Position>();
    auto archetype = world.GetArchetypes<Position, Velocity, Transform>();
    EXPECT_EQ(archetype.size(), 1);
    EXPECT_EQ(archetype.front()->NumEntities(), 2);
}

TEST(EcsTest, ModifyEntity) {
    World world("EcsTest.ModifyEntity");

    struct Position {
        int value = 1;
    };

    auto pos_entity = world.CreateEntity<Position>();
    {
        Position& component = world.AccessEntity<Position>(pos_entity)->get();
        component.value     = 2;
    }
    {
        Position& component = world.AccessEntity<Position>(pos_entity)->get();
        EXPECT_EQ(component.value, 2);
    }
}

TEST(EcsTest, RegisterSystem) {
    World world("EcsTest.RegisterSystem");

    struct Position {
        int value = 1;
    };
    struct Velocity {
        int value = 1;
    };

    struct Mover {
        static void OnUpdate(Schedule& schedule, std::chrono::duration<double> delta) {
            schedule
                .Request(
                    "Update Position",
                    [](Position& position) {
                        position.value++;
                    })
                .Request("Update Velocity", [](Velocity& velocity) {
                    velocity.value = 3;
                });
        }
    };

    auto entity = world.CreateEntity<Position, Velocity>();
    world.RegisterSystem<Mover>();
    world.Update();

    auto pos = world.AccessEntity<Position>(entity)->get();
    auto vel = world.AccessEntity<Velocity>(entity)->get();
    EXPECT_EQ(pos.value, 2);
    EXPECT_EQ(vel.value, 3);
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::err);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}