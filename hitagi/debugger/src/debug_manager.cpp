#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/resource/mesh_factory.hpp>
#include <hitagi/resource/asset_manager.hpp>

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

    m_LineMaterial = g_AssetManager->ImportMaterial("assets/material/debug_line.yaml");

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
    m_TimingInfo.clear();
}

void DebugManager::ToggleDebugInfo() {
    m_DrawDebugInfo = !m_DrawDebugInfo;
}

void DebugManager::DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    Geometry line(std::make_shared<Transform>());
    line.meshes.emplace_back(MeshFactory::Line(from, to, color));

    AddPrimitive(std::move(line), duration, depth_enabled);
}

void DebugManager::DrawAxis(const mat4f& transform, bool depth_enabled) {
    const vec3f origin{0.0f, 0.0f, 0.0f};
    const vec3f x{1.0f, 0.0f, 0.0f};
    const vec3f y{0.0f, 1.0f, 0.0f};
    const vec3f z{0.0f, 0.0f, 1.0f};

    auto x_axis     = MeshFactory::Line(origin, x, vec4f(1, 0, 0, 1));
    auto y_axis     = MeshFactory::Line(origin, y, vec4f(0, 1, 0, 1));
    auto z_axis     = MeshFactory::Line(origin, z, vec4f(0, 0, 1, 1));
    x_axis.material = m_LineMaterial;
    y_axis.material = m_LineMaterial;
    z_axis.material = m_LineMaterial;

    Geometry axis(std::make_shared<Transform>(decompose(transform)));
    axis.meshes.emplace_back(x_axis);
    axis.meshes.emplace_back(y_axis);
    axis.meshes.emplace_back(z_axis);

    AddPrimitive(std::move(axis), std::chrono::seconds(0), depth_enabled);
}

void DebugManager::DrawBox(const mat4f& transform, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    Geometry box(std::make_shared<Transform>(decompose(transform)));
    auto     mesh = MeshFactory::BoxWireframe(vec3f(-0.5f, -0.5f, -0.5f), vec3f(0.5f, 0.5f, 0.5f), color);
    mesh.material = m_LineMaterial;
    box.meshes.emplace_back(mesh);

    AddPrimitive(std::move(box), duration, depth_enabled);
}

void DebugManager::AddPrimitive(Geometry&& geometry, std::chrono::seconds duration, bool depth_enabled) {
    m_DebugPrimitives.emplace_back(DebugPrimitive{std::move(geometry), std::chrono::high_resolution_clock::now() + duration});
    // make min heap
    std::push_heap(m_DebugPrimitives.begin(), m_DebugPrimitives.end(), cmp);
}

}  // namespace hitagi::debugger