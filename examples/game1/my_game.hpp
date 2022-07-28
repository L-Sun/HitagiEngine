#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>

class MyGame : public hitagi::RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;

private:
    hitagi::core::Clock m_Clock;
};