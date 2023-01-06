#include <hitagi/engine.hpp>
#include "playground.hpp"

auto main(int argc, char** argv) -> int {
    hitagi::Engine engine;
    engine.AddSubModule(std::make_unique<Playground>(engine));

    while (!engine.App().IsQuit()) engine.Tick();

    return 0;
}