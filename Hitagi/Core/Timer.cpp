#include "Timer.hpp"

namespace Hitagi::Core {

int Clock::Initialize() {
    m_DeltaTime  = std::chrono::duration<double>::zero();
    m_PausedTime = std::chrono::duration<double>::zero();
    m_BaseTime   = std::chrono::high_resolution_clock::now();
    m_Paused     = true;
    m_StopTime   = m_BaseTime;

    return 0;
}
void Clock::Finalize() {}

std::chrono::duration<double> Clock::deltaTime() const { return m_DeltaTime; }

std::chrono::duration<double> Clock::totalTime() const {
    if (m_Paused) {
        return (m_StopTime - m_BaseTime) - m_PausedTime;
    } else {
        return (m_TickTime - m_BaseTime) - m_PausedTime;
    }
}

std::chrono::high_resolution_clock::time_point Clock::tickTime() const { return m_TickTime; }

void Clock::Tick() {
    if (m_Paused) {
        m_DeltaTime = std::chrono::duration<double>::zero();
        return;
    }
    m_TickTime  = std::chrono::high_resolution_clock::now();
    m_DeltaTime = m_TickTime - m_PrevTime;
    m_PrevTime  = m_TickTime;
}

void Clock::Start() {
    if (m_Paused) {
        auto now = std::chrono::high_resolution_clock::now();
        m_PausedTime += now - m_StopTime;
        m_PrevTime = now;
        m_Paused   = false;
    }
}

void Clock::Pause() {
    if (!m_Paused) {
        m_StopTime = std::chrono::high_resolution_clock::now();
        m_Paused   = true;
    }
}

void Clock::Reset() {
    m_BaseTime = std::chrono::high_resolution_clock::now();
    m_Paused   = true;
}
}  // namespace Hitagi::Core