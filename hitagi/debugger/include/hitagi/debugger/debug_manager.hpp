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
    void DrawPrimitive();

    std::pmr::vector<resource::Renderable>       m_DebugDrawItems;
    std::pmr::vector<resource::MaterialInstance> m_DrawItemColors;

    resource::Mesh                      m_MeshPrototype;
    std::shared_ptr<resource::Material> m_LineMaterial;

    bool m_DrawDebugInfo = true;
};

}  // namespace hitagi::debugger
namespace hitagi {
extern debugger::DebugManager* debug_manager;
}