#pragma once
#include <chrono>
#include <string>
#include "IRuntimeModule.hpp"

namespace Hitagi::Core {
class Clock : public IRuntimeModule {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    std::chrono::duration<double>                  totalTime() const;
    std::chrono::duration<double>                  deltaTime() const;
    std::chrono::high_resolution_clock::time_point GetBaseTime() const;
    std::chrono::high_resolution_clock::time_point tickTime() const;

    void Reset();
    void Start();
    void Pause();

private:
    std::chrono::duration<double> m_DeltaTime;
    std::chrono::duration<double> m_PausedTime;

    std::chrono::high_resolution_clock::time_point m_BaseTime;
    std::chrono::high_resolution_clock::time_point m_StopTime;
    std::chrono::high_resolution_clock::time_point m_PrevTime;
    std::chrono::high_resolution_clock::time_point m_TickTime;

    bool m_Paused = true;
};

}  // namespace Hitagi::Core
