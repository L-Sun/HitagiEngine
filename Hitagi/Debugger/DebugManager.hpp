#pragma once
#include "IRuntimeModule.hpp"
#include "Geometry.hpp"
#include "SceneNode.hpp"

#include <Math/Transform.hpp>

#include <chrono>
#include <functional>

namespace Hitagi::Debugger {
struct DebugPrimitive {
    std::shared_ptr<Asset::GeometryNode>           geometry_node;
    Math::vec4f                                    color;
    std::chrono::high_resolution_clock::time_point expires_at;
};

class DebugManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;
    void ToggleDebugInfo();
    void DrawDebugInfo();

    void DrawLine(const Math::vec3f& from, const Math::vec3f& to, const Math::vec4f& color, std::chrono::seconds duration = std::chrono::seconds(0), bool depth_enabled = true);
    void DrawAxis(const Math::mat4f& transform, bool depth_enabled = true);
    void DrawBox(const Math::mat4f& transform, const Math::vec4f& color, std::chrono::seconds duration = std::chrono::seconds(0), bool depth_enabled = true);

    template <typename Func>
    void Profiler(const std::string& name, Func&& func) {
        Core::Clock clock;
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
    void AddPrimitive(std::shared_ptr<Asset::Geometry> geometry, const Math::mat4f& transform, const Math::vec4f& color, std::chrono::seconds duration, bool depth_enabled);

    void ShowProfilerInfo();

    std::vector<DebugPrimitive>      m_DebugPrimitives;
    std::shared_ptr<Asset::Geometry> m_DebugLine;
    std::shared_ptr<Asset::Geometry> m_DebugBox;

    std::unordered_map<std::string, std::chrono::duration<double>> m_TimingInfo;

    bool m_DrawDebugInfo = true;
};

}  // namespace Hitagi::Debugger
namespace Hitagi {
extern std::unique_ptr<Debugger::DebugManager> g_DebugManager;
}