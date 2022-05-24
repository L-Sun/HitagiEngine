#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/resource/geometry.hpp>
#include <hitagi/math/transform.hpp>

#include <chrono>
#include <functional>

namespace hitagi::debugger {
struct DebugPrimitive {
    resource::Geometry                             geometry;
    std::chrono::high_resolution_clock::time_point expires_at;
};

class DebugManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;
    void ToggleDebugInfo();
    void DrawDebugInfo();

    void DrawLine(const math::vec3f& from, const math::vec3f& to, const math::vec4f& color, std::chrono::seconds duration = std::chrono::seconds(0), bool depth_enabled = true);
    void DrawAxis(const math::mat4f& transform, bool depth_enabled = true);
    void DrawBox(const math::mat4f& transform, const math::vec4f& color, std::chrono::seconds duration = std::chrono::seconds(0), bool depth_enabled = true);

    inline auto GetDebugPrimitiveForRender() const noexcept {
        auto result = std::cref(m_DebugPrimitives);
        return m_DrawDebugInfo ? std::optional{result} : std::nullopt;
    };

protected:
    void AddPrimitive(resource::Geometry&& geometry, std::chrono::seconds duration, bool depth_enabled);

    std::pmr::vector<DebugPrimitive> m_DebugPrimitives;

    std::unordered_map<std::string, std::chrono::duration<double>> m_TimingInfo;

    std::shared_ptr<resource::MaterialInstance> m_LineMaterial;

    bool m_DrawDebugInfo = true;
};

}  // namespace hitagi::debugger
namespace hitagi {
extern std::unique_ptr<debugger::DebugManager> g_DebugManager;
}