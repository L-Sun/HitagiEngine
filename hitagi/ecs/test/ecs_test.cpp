#include <hitagi/utils/test.hpp>
#include <hitagi/ecs/ecs_manager.hpp>
#include <vector>
#include "spdlog/common.h"
#include "spdlog/spdlog.h"

using namespace hitagi::ecs;

TEST(EcsTest, CreateEntity) {
    EcsManager ecs_mgr;
    World&     world = *ecs_mgr.CreateEcsWorld("EcsTest.CreateEntity");

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
}

TEST(EcsTest, ModifyEntity) {
    EcsManager ecs_mgr;
    World&     world = *ecs_mgr.CreateEcsWorld("EcsTest.ModifyEntity");

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
    EcsManager ecs_mgr;
    World&     world = *ecs_mgr.CreateEcsWorld("EcsTest.RegisterSystem");

    struct Position {
        int value = 1;
    };
    struct Velocity {
        int value = 1;
    };

    struct Mover {
        static void OnUpdate(Schedule& schedule, std::chrono::duration<double> delta) {
            schedule
                .Register(
                    "Update Position",
                    [](Position& position) {
                        position.value++;
                    })
                .Register("Update Velocity", [](Velocity& velocity) {
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