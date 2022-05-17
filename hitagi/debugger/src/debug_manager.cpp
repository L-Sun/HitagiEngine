#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/resource/mesh_factory.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/chrono.h>

using namespace hitagi::math;
using namespace hitagi::resource;

namespace hitagi {
std::unique_ptr<debugger::DebugManager> g_DebugManager = std::make_unique<debugger::DebugManager>();
}
namespace hitagi::debugger {
constexpr auto cmp = [](const DebugPrimitive& lhs, const DebugPrimitive& rhs) -> bool {
    return lhs.expires_at > rhs.expires_at;
};

int DebugManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("DebugManager");
    m_Logger->info("Initialize...");
    return 0;
}

void DebugManager::Finalize() {
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void DebugManager::Tick() {
    while (!m_DebugPrimitives.empty() && m_DebugPrimitives.back().expires_at < std::chrono::high_resolution_clock::now()) {
        std::pop_heap(m_DebugPrimitives.begin(), m_DebugPrimitives.end(), cmp);
        m_DebugPrimitives.pop_back();
    }
    ShowProfilerInfo();
    m_TimingInfo.clear();
}

void DebugManager::ToggleDebugInfo() {
    m_DrawDebugInfo = !m_DrawDebugInfo;
}

void DebugManager::DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    Geometry line(std::make_shared<Transform>());
    line.AddMesh(MeshFactory::Line(from, to, color));

    AddPrimitive(std::move(line), duration, depth_enabled);
}

void DebugManager::DrawAxis(const mat4f& transform, bool depth_enabled) {
    const vec3f origin{0.0f, 0.0f, 0.0f};
    const vec3f x{1.0f, 0.0f, 0.0f};
    const vec3f y{0.0f, 1.0f, 0.0f};
    const vec3f z{0.0f, 0.0f, 1.0f};

    Geometry axis(std::make_shared<Transform>(decompose(transform)));
    axis.AddMesh(MeshFactory::Line(origin, x, vec4f(1, 0, 0, 1)));
    axis.AddMesh(MeshFactory::Line(origin, x, vec4f(0, 1, 0, 1)));
    axis.AddMesh(MeshFactory::Line(origin, x, vec4f(0, 0, 1, 1)));

    AddPrimitive(std::move(axis), std::chrono::seconds(0), depth_enabled);
}

void DebugManager::DrawBox(const mat4f& transform, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    Geometry box(std::make_shared<Transform>(decompose(transform)));
    box.AddMesh(MeshFactory::BoxWireframe(vec3f(-0.5f, -0.5f, -0.5f), vec3f(0.5f, 0.5f, 0.5f), color));

    AddPrimitive(std::move(box), duration, depth_enabled);
}

void DebugManager::AddPrimitive(Geometry&& geometry, std::chrono::seconds duration, bool depth_enabled) {
    m_DebugPrimitives.emplace_back(DebugPrimitive{std::move(geometry), std::chrono::high_resolution_clock::now() + duration});
    // make min heap
    std::push_heap(m_DebugPrimitives.begin(), m_DebugPrimitives.end(), cmp);
}

}  // namespace hitagi::debugger