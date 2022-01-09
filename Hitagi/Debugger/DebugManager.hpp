#pragma once
#include "IRuntimeModule.hpp"
#include "HitagiMath.hpp"
#include "Geometry.hpp"
#include "SceneNode.hpp"

#include <chrono>
#include <functional>

namespace Hitagi::Debugger {
struct DebugPrimitive {
    std::shared_ptr<Asset::GeometryNode>           geometry_node;
    vec4f                                          color;
    std::chrono::high_resolution_clock::time_point expires_at;
};

class DebugManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;
    void ToggleDebugInfo();
    void DrawDebugInfo();

    void DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, std::chrono::seconds duration = std::chrono::seconds(0), bool depth_enabled = true);
    void DrawAxis(const mat4f& transform, bool depth_enabled = true);
    void DrawBox(const mat4f& transform, const vec4f& color, std::chrono::seconds duration = std::chrono::seconds(0), bool depth_enabled = true);

    inline auto GetDebugPrimitiveForRender() const noexcept {
        auto result = std::cref(m_DebugPrimitives);
        return m_DrawDebugInfo ? std::optional{result} : std::nullopt;
    };

protected:
    void AddPrimitive(std::shared_ptr<Asset::Geometry> geometry, const mat4f& transform, const vec4f& color, std::chrono::seconds duration, bool depth_enabled);

    std::vector<DebugPrimitive>      m_DebugPrimitives;
    std::shared_ptr<Asset::Geometry> m_DebugLine;
    std::shared_ptr<Asset::Geometry> m_DebugBox;

    bool m_DrawDebugInfo = true;
};

}  // namespace Hitagi::Debugger
namespace Hitagi {
extern std::unique_ptr<Debugger::DebugManager> g_DebugManager;
}