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

    inline auto GetDebugPrimitiveForRender() const noexcept {
        auto result = std::cref(m_DebugPrimitives);
        return m_DrawDebugInfo ? std::optional{result} : std::nullopt;
    };

protected:
    void AddPrimitive(resource::Geometry&& geometry, std::chrono::seconds duration, bool depth_enabled);
    void RetiredPrimitive();
    void DrawPrimitive() const;

    std::pmr::vector<DebugPrimitive>                          m_DebugDrawItems;
    std::pmr::unordered_map<std::pmr::string, resource::Mesh> m_DebugPrimitives;

    std::shared_ptr<resource::MaterialInstance> m_LineMaterial;

    bool m_DrawDebugInfo = true;
};

}  // namespace hitagi::debugger
namespace hitagi {
extern debugger::DebugManager* debug_manager;
}