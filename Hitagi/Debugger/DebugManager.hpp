#pragma once
#include "IRuntimeModule.hpp"
#include "Geometry.hpp"
#include "SceneNode.hpp"

#include <Hitagi/Math/Transform.hpp>

#include <chrono>
#include <functional>

namespace hitagi::debugger {
struct DebugPrimitive {
    std::shared_ptr<asset::GeometryNode>           geometry_node;
    math::vec4f                                    color;
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

    template <typename Func>
    void Profiler(const std::string& name, Func&& func) {
        core::Clock clock;
        clock.Start();
        clock.Tick();
        func();
        if (m_TimingInfo.count(name) == 0)
            m_TimingInfo[name] = clock.DeltaTime();
        else
            m_TimingInfo[name] += clock.DeltaTime();
    }

    inline auto GetDebugPrimitiveForRender() const noexcept {
        auto result = std::cref(m_DebugPrimitives);
        return m_DrawDebugInfo ? std::optional{result} : std::nullopt;
    };

protected:
    void AddPrimitive(std::shared_ptr<asset::Geometry> geometry, const math::mat4f& transform, const math::vec4f& color, std::chrono::seconds duration, bool depth_enabled);

    void ShowProfilerInfo();

    std::vector<DebugPrimitive>      m_DebugPrimitives;
    std::shared_ptr<asset::Geometry> m_DebugLine;
    std::shared_ptr<asset::Geometry> m_DebugBox;

    std::unordered_map<std::string, std::chrono::duration<double>> m_TimingInfo;

    bool m_DrawDebugInfo = true;
};

}  // namespace hitagi::debugger
namespace hitagi {
extern std::unique_ptr<debugger::DebugManager> g_DebugManager;
}