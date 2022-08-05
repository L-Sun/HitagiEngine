#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/timer.hpp>
#include <hitagi/resource/renderable.hpp>

#include <chrono>
#include <functional>

namespace hitagi::debugger {
class DebugManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;
    void Tick() final;
    void ToggleDebugInfo();
    void DrawDebugInfo();

    void DrawLine(const math::vec3f& from, const math::vec3f& to, const math::vec4f& color, bool depth_enabled = true);
    void DrawAxis(const math::mat4f& transform, bool depth_enabled = true);
    void DrawBox(const math::mat4f& transform, const math::vec4f& color, bool depth_enabled = true);

    inline std::size_t GetNumPrimitives() const noexcept { return m_DebugDrawItems.size(); }

protected:
    void AddPrimitive(resource::Mesh mesh, resource::Transform transform, bool depth_enabled);
    void DrawPrimitive();

    std::pmr::vector<resource::Renderable> m_DebugDrawItems;

    resource::Mesh m_MeshBuffer;
    // Indicate the current vertex used
    std::size_t m_VertexOffset = 0;
    std::size_t m_IndexOffset  = 0;

    resource::MaterialInstance m_LineMaterialInstance;

    bool m_DrawDebugInfo = true;
};

}  // namespace hitagi::debugger
namespace hitagi {
extern debugger::DebugManager* debug_manager;
}