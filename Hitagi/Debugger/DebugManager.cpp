#include "DebugManager.hpp"
#include "GeometryFactory.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <iostream>
#include <algorithm>

namespace Hitagi {
std::unique_ptr<Debugger::DebugManager> g_DebugManager = std::make_unique<Debugger::DebugManager>();
}
namespace Hitagi::Debugger {
constexpr auto cmp = [](const DebugPrimitive& lhs, const DebugPrimitive& rhs) -> bool {
    return lhs.expires_at > rhs.expires_at;
};

int DebugManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("DebugManager");
    m_Logger->info("Initialize...");
    return 0;
}

void DebugManager::Finalize() {
    m_DebugLine = nullptr;
    m_DebugBox  = nullptr;
    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void DebugManager::Tick() {
    while (!m_DebugPrimitives.empty() && m_DebugPrimitives.back().expires_at < std::chrono::high_resolution_clock::now()) {
        std::pop_heap(m_DebugPrimitives.begin(), m_DebugPrimitives.end(), cmp);
        m_DebugPrimitives.pop_back();
    }
}

void DebugManager::ToggleDebugInfo() {
    m_DrawDebugInfo = !m_DrawDebugInfo;
}

void DebugManager::DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    if (m_DebugLine == nullptr)
        m_DebugLine = Asset::GeometryFactory::Line(vec3f(1.0f, 0.0f, 0.0f), vec3f(0.0f, 1.0f, 0.0f));

    // T*(p1,p2,p3,p4) = (from, to, q3, q4)
    // so T = (from, to, q3, q4) * (p1,p2,p3,p4)^-1
    // we set  p1 = (1, 0, 0, 1), p2 = (0, 1, 0, 1), p3 = (0, 0, 1, 1), p4 = (0, 0, 0, 1)
    // and set q3 = (0, 0, 1, 1), q4 = (0, 0, 0, 1)
    mat4f p1p2p3p4 = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
    };
    mat4f q1q2q3q4 = {
        {from.x, to.x, 0.0f, 0.0f},
        {from.y, to.y, 0.0f, 0.0f},
        {from.z, to.z, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
    };
    mat4f transform = q1q2q3q4 * inverse(p1p2p3p4);
    AddPrimitive(m_DebugLine, transform, color, duration, depth_enabled);
}
void DebugManager::DrawAxis(const mat4f& transform, bool depth_enabled) {
    const vec3f origin{0.0f, 0.0f, 0.0f};
    const vec3f x{1.0f, 0.0f, 0.0f};
    const vec3f y{0.0f, 1.0f, 0.0f};
    const vec3f z{0.0f, 0.0f, 1.0f};
    DrawLine(origin, x, vec4f(1, 0, 0, 1), std::chrono::seconds(0), depth_enabled);
    DrawLine(origin, y, vec4f(0, 1, 0, 1), std::chrono::seconds(0), depth_enabled);
    DrawLine(origin, z, vec4f(0, 0, 1, 1), std::chrono::seconds(0), depth_enabled);
}

void DebugManager::DrawBox(const mat4f& transform, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    if (m_DebugBox == nullptr)
        m_DebugBox = Asset::GeometryFactory::Box(vec3f(-0.5f, -0.5f, -0.5f), vec3f(0.5f, 0.5f, 0.5f));
    AddPrimitive(m_DebugBox, transform, color, duration, depth_enabled);
}

void DebugManager::AddPrimitive(std::shared_ptr<Asset::Geometry> geometry, const mat4f& transform, const vec4f& color, std::chrono::seconds duration, bool depth_enabled) {
    auto node = std::make_shared<Asset::GeometryNode>();
    node->SetSceneObjectRef(geometry);
    node->ApplyTransform(transform);
    m_DebugPrimitives.emplace_back(DebugPrimitive{node, color, std::chrono::high_resolution_clock::now() + duration});
    // make min heap
    std::push_heap(m_DebugPrimitives.begin(), m_DebugPrimitives.end(), cmp);
}

}  // namespace Hitagi::Debugger