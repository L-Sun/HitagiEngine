#pragma once
#include "IRuntimeModule.hpp"
#include "HitagiMath.hpp"

#include <chrono>

namespace Hitagi::Debugger {
struct DebugPrimitive {
    Geometry geometry;
    vec4f    color;
};

class DebugManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;
    void ToggleDebugInfo();
    void DrawDebugInfo();

    void DrawLine(const Line& line, const vec4f& color, std::chrono::duration<double> duration, bool depthEnabled = true);

    const std::vector<DebugPrimitive>& GetDebugPrimitiveForRender() const noexcept { return m_DebugPrimitives; };

protected:
    std::vector<DebugPrimitive> m_DebugPrimitives;
    bool                        m_DrawDebugInfo = false;
};

}  // namespace Hitagi::Debugger
namespace Hitagi {
extern std::unique_ptr<Debugger::DebugManager> g_DebugManager;
}