#include <hitagi/ecs/world.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/math/transform.hpp>
#include <hitagi/utils/test.hpp>

using namespace hitagi;

static void ECS_Update(benchmark::State& state) {
    ecs::World world(fmt::format("ECS_Update-{}", state.thread_index()));

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

    world.RegisterSystem<MoveSystem>("MoveSystem");
    world.RegisterSystem<ClockSystem>("ClockSystem");

    auto entities = world.GetEntityManager().CreateMany<Moveable, core::Clock>(1'000'000);
    for (const auto& entity : entities) {
        world.GetEntityManager().GetComponent<core::Clock>(entity)->get().Start();
    }

    for (auto _ : state) {
        world.Update();
    }
}
BENCHMARK(ECS_Update);

BENCHMARK_MAIN();
