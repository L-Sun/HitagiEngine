#pragma once
#include <chrono>
#include <string>

namespace hitagi::core {
class Clock {
public:
    Clock();

    std::chrono::duration<double>                  TotalTime() const;
    std::chrono::duration<double>                  DeltaTime() const;
    std::chrono::high_resolution_clock::time_point GetBaseTime() const;
    std::chrono::high_resolution_clock::time_point TickTime() const;

    void        Reset();
    void        Start();
    void        Tick();
    void        Pause();
    inline bool IsPaused() const noexcept { return m_Paused; }

private:
    std::chrono::duration<double> m_PausedTime = std::chrono::duration<double>::zero();

    std::chrono::high_resolution_clock::time_point m_BaseTime;
    std::chrono::high_resolution_clock::time_point m_StopTime;
    std::chrono::high_resolution_clock::time_point m_TickTime;

    std::chrono::duration<double> m_Deltatime = std::chrono::duration<double>::zero();

    bool m_Paused = true;
};

}  // namespace hitagi::core
