#include "Timer.hpp"

namespace hitagi::core {

Clock::Clock()
    : m_BaseTime(std::chrono::high_resolution_clock::now()), m_StopTime(m_BaseTime) {}

auto Clock::DeltaTime() const -> std::chrono::duration<double> {
    if (m_Paused) {
        return m_StopTime - m_TickTime;
    }
    return std::chrono::high_resolution_clock::now() - m_TickTime;
}

auto Clock::TotalTime() const -> std::chrono::duration<double> {
    if (m_Paused) {
        return (m_StopTime - m_BaseTime) - m_PausedTime;
    } else {
        return (m_TickTime - m_BaseTime) - m_PausedTime;
    }
}

std::chrono::high_resolution_clock::time_point Clock::TickTime() const { return m_TickTime; }

void Clock::Tick() {
    if (m_Paused) {
        return;
    }
    m_TickTime = std::chrono::high_resolution_clock::now();
}
auto Clock::GetBaseTime() const -> std::chrono::high_resolution_clock::time_point {
    return m_BaseTime;
}

void Clock::Start() {
    if (m_Paused) {
        auto now = std::chrono::high_resolution_clock::now();
        m_PausedTime += now - m_StopTime;
        m_TickTime = now;
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
    m_BaseTime   = std::chrono::high_resolution_clock::now();
    m_PausedTime = std::chrono::duration<double>::zero();
    m_StopTime   = m_BaseTime;
}
}  // namespace hitagi::core