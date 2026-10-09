#include "interop/benchmark_macros.hpp"
#include "test_macros.hpp"
import interop.gtest;
import interop.benchmark;

import std;
import ecs;
import core;
import math;
import test_utils;

using namespace hitagi;

static void ECS_Update(benchmark::State& state) {
    core::JobSystem job_system;
    ecs::World      world(std::format("ECS_Update-{}", state.thread_index()));

    struct Moveable {
        math::vec3f position;
        math::vec3f velocity;
    };

    struct MoveSystem {
        static void OnUpdate(ecs::Schedule& schedule) {
            schedule.Request("Move", [=](Moveable& data, const core::Clock& clock) {
                data.position += data.velocity * clock.DeltaTime().count();
                data.velocity *= 1.01;
            });
        }
    };

    struct ClockSystem {
        static void OnUpdate(ecs::Schedule& schedule) {
            schedule.Request("ClockTick", [](core::Clock& clock) {
                clock.Tick();
            });
        }
    };

    world.RegisterSystem<MoveSystem>();
    world.RegisterSystem<ClockSystem>();

    auto entities = world.GetEntityManager().CreateMany<Moveable, core::Clock>(1'000'000);

    for (auto& entity : entities) {
        entity.Get<core::Clock>().Start();
    }

    for (auto _ : state) {
        world.Update(job_system);
    }
}
BENCHMARK(ECS_Update);

BENCHMARK_MAIN();
