#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/core/memory_manager.hpp>

using namespace hitagi::math;

namespace hitagi {
debugger::DebugManager* debug_manager = nullptr;
}
namespace hitagi::debugger {

void DebugManager::Tick() {
    if (m_DrawDebugInfo) {
        // TODO
    }

    m_DrawItems.clear();
}

void DebugManager::DrawLine(const vec3f& from, const vec3f& to, const Color& color, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;
    vec3f dir = normalize(to - from);

    vec3f axis(0);
    // The axis that the angle between it and dir is maximum
    axis[min_index(dir)] = 1;

    m_DrawItems.emplace_back(DrawItem{
        .transform       = translate(from) * rotate(std::numbers::pi_v<float>, normalize(dir + axis)) * scale((to - from).norm()),
        .color           = color,
        .primitive_index = 0,
    });
}

void DebugManager::DrawAxis(const math::mat4f& transform, bool depth_enabled) {
    // TODO
}

void DebugManager::DrawBox(const mat4f& transform, const Color& color, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;

    m_DrawItems.emplace_back(DrawItem{
        .transform       = transform,
        .color           = color,
        .primitive_index = 1,
    });
}

}  // namespace hitagi::debugger