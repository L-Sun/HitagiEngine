#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/utils/test.hpp>
#include <hitagi/math/vector.hpp>
#include "benchmark/benchmark.h"

#include <spdlog/spdlog.h>

using namespace hitagi;

static void BM_ECS(benchmark::State& state) {
    auto world = ecs::World("Benchmark");

    struct Moveable {
        math::vec3f position;
        math::vec3f velocity;
    };

    struct Mover {
        static void OnUpdate(ecs::Schedule& schedule, std::chrono::duration<double> delta) {
            schedule.Request("Move", [=](Moveable& data) {
                data.position += data.velocity * delta.count();
                data.velocity *= 1.01;
            });
        }
    };

    world.RegisterSystem<Mover>();

    world.CreateEntities<Moveable>(1000000);

    for (auto _ : state) {
        world.Update();
    }
}
BENCHMARK(BM_ECS);

BENCHMARK_MAIN();
