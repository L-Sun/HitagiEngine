#pragma once
#include "IRuntimeModule.hpp"
#include "HitagiMath.hpp"

#include <chrono>

namespace Hitagi::Debugger {
struct DebugPrimitive {
    std::unique_ptr<Geometry>                      geometry;
    vec4f                                          color;
    mat4f                                          transform;
    std::chrono::high_resolution_clock::time_point expires_at;
};

class DebugManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;
    void ToggleDebugInfo();
    void DrawDebugInfo();

    void DrawLine(const Line& line, const vec4f& color, std::chrono::seconds duration = std::chrono::seconds(0), bool depth_enabled = true);

    const std::vector<DebugPrimitive>& GetDebugPrimitiveForRender() const noexcept { return m_DebugPrimitives; };

protected:
    void AddPrimitive(std::unique_ptr<Geometry> geometry, const vec4f& color,const mat4f& transform, std::chrono::seconds duration, bool depth_enabled);

    std::vector<DebugPrimitive> m_DebugPrimitives;

    bool m_DrawDebugInfo = false;
};

}  // namespace Hitagi::Debugger
namespace Hitagi {
extern std::unique_ptr<Debugger::DebugManager> g_DebugManager;
}