#pragma once
#include <hitagi/core/core.hpp>

namespace hitagi {
class Profiler : public RuntimeModule {
public:
    std::string_view GetName() const final { return "Profiler"; }

    bool Initialize() final;
    void Tick() final;

private:
    bool        m_Open = true;
    core::Clock m_Clock;
};
}  // namespace hitagi