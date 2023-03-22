#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/math/transform.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

#include <unordered_map>

namespace hitagi::debugger {
class DebugManager : public RuntimeModule {
public:
    DebugManager() : RuntimeModule("DebugManager") {}
    void Tick() final;

    inline void EnableDebugDraw() noexcept { m_DrawDebugInfo = true; }
    inline void DisableDebugDraw() noexcept { m_DrawDebugInfo = false; }

    void DrawLine(const math::vec3f& from, const math::vec3f& to, const math::vec4f& color, bool depth_enabled = true);
    void DrawAxis(const math::mat4f& transform, bool depth_enabled = true);
    void DrawBox(const math::mat4f& transform, const math::vec4f& color, bool depth_enabled = true);

protected:
    struct GfxData {
        std::shared_ptr<gfx::GPUBuffer>      vertices;
        std::shared_ptr<gfx::GPUBuffer>      indices;
        std::shared_ptr<gfx::RenderPipeline> m_DebugPipeline;
    } m_GfxData;

    struct DebugPrimitive {
        std::size_t index_count;
        std::size_t index_start;
        std::size_t base_vertex;
    };
    std::pmr::vector<DebugPrimitive> m_Primitives;

    struct DrawItem {
        math::mat4f   transform;
        math::vec4f   color;
        std::uint32_t primitive_index;
    };
    std::pmr::vector<DrawItem> m_DrawItems;

    bool m_DrawDebugInfo = true;
};

}  // namespace hitagi::debugger
namespace hitagi {
extern debugger::DebugManager* debug_manager;
}