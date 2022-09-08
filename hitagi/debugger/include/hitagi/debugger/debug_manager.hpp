#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/math/transform.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/graphics/draw_data.hpp>

#include <unordered_map>

namespace hitagi::debugger {
class DebugManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;

    inline std::string_view GetName() const noexcept final { return "DebugManager"; }

    inline void EnableDebugDraw() noexcept { m_DrawDebugInfo = true; }
    inline void DisableDebugDraw() noexcept { m_DrawDebugInfo = false; }

    void DrawLine(const math::vec3f& from, const math::vec3f& to, const math::vec4f& color, bool depth_enabled = true);
    void DrawAxis(const math::mat4f& transform, bool depth_enabled = true);
    void DrawBox(const math::mat4f& transform, const math::vec4f& color, bool depth_enabled = true);
    void DrawMesh(const math::mat4f& transform, const resource::Mesh& mesh);

protected:
    bool m_DrawDebugInfo = true;

    resource::Mesh     m_DebugPrimitives;
    gfx::DebugDrawData m_DebugDrawData;
};

}  // namespace hitagi::debugger
namespace hitagi {
extern debugger::DebugManager* debug_manager;
}