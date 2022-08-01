#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/resource/renderable.hpp>

#include <chrono>
#include <functional>

namespace hitagi::debugger {
struct DebugPrimitive : resource::Renderable {
    std::chrono::high_resolution_clock::time_point expires_at;
    bool                                           dirty = true;
};

class DebugManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;
    void ToggleDebugInfo();
    void DrawDebugInfo();

    void DrawLine(const math::vec3f& from, const math::vec3f& to, const math::vec4f& color, std::chrono::seconds duration = std::chrono::seconds(0), bool depth_enabled = true);
    void DrawAxis(const math::mat4f& transform, bool depth_enabled = true);
    void DrawBox(const math::mat4f& transform, const math::vec4f& color, std::chrono::seconds duration = std::chrono::seconds(0), bool depth_enabled = true);

protected:
    void AddPrimitive(const resource::Mesh& mesh, resource::Transform transform, std::chrono::seconds duration, bool depth_enabled);
    void RetiredPrimitive();
    void DrawPrimitive() const;

    std::pmr::vector<DebugPrimitive>                          m_DebugDrawItems;
    std::pmr::unordered_map<std::pmr::string, resource::Mesh> m_DebugPrimitivePrototypes;

    resource::MaterialInstance m_LineMaterialInstance;

    bool m_DrawDebugInfo = true;
};

}  // namespace hitagi::debugger
namespace hitagi {
extern debugger::DebugManager* debug_manager;
}