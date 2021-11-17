#include "DebugManager.hpp"

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
}

void DebugManager::DrawLine(const Line& line, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    AddPrimitive(std::make_unique<Line>(line), color, mat4f(1.0f), duration, depth_enabled);
}
void DebugManager::DrawAxis(const mat4f& transform, bool depth_enabled) {
    const vec3f origin{0.0f, 0.0f, 0.0f};
    const vec3f x{10.0f, 0.0f, 0.0f};
    const vec3f y{0.0f, 10.0f, 0.0f};
    const vec3f z{0.0f, 0.0f, 10.0f};

    AddPrimitive(std::make_unique<Line>(origin, x), vec4f(1, 0, 0, 1), transform, std::chrono::seconds(0), depth_enabled);
    AddPrimitive(std::make_unique<Line>(origin, y), vec4f(0, 1, 0, 1), transform, std::chrono::seconds(0), depth_enabled);
    AddPrimitive(std::make_unique<Line>(origin, z), vec4f(0, 0, 1, 1), transform, std::chrono::seconds(0), depth_enabled);
}

void DebugManager::AddPrimitive(std::unique_ptr<Geometry> geometry, const vec4f& color, const mat4f& transform, std::chrono::seconds duration, bool depth_enabled) {
    m_DebugPrimitives.emplace_back(DebugPrimitive{std::move(geometry), color, transform, std::chrono::high_resolution_clock::now() + duration});
    // make min heap
    std::push_heap(m_DebugPrimitives.begin(), m_DebugPrimitives.end(), cmp);
}

}  // namespace Hitagi::Debugger