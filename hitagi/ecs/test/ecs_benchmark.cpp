#include <hitagi/utils/test.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/ecs/ecs_manager.hpp>
#include <hitagi/math/vector.hpp>

using namespace hitagi;

static void BM_ECS(benchmark::State& state) {
    ecs::EcsManager mgr;

    auto world = mgr.CreateEcsWorld("Benchmark");

    struct Moveable {
        math::vec3f position;
        math::vec3f velocity;
    };

    struct Mover {
        static void OnUpdate(ecs::Schedule& schedule, std::chrono::duration<double> delta) {
            schedule.Register("Move", [&](Moveable& data) {
                data.position += data.velocity * delta.count();
                data.velocity *= 1.01;
            });
        }
    };

    world->RegisterSystem<Mover>();

    for (auto _ : state) {
        if (world->NumEntities() != 100000)
            world->CreateEntity<Moveable>();
        world->Update();
    }
}
BENCHMARK(BM_ECS);

int main(int argc, char** argv) {
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    g_MemoryManager->Initialize();
    ::benchmark::RunSpecifiedBenchmarks();
    g_MemoryManager->Finalize();
    ::benchmark::Shutdown();
    return 0;
}
