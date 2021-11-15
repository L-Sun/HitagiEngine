#include "DebugManager.hpp"
#include "IPhysicsManager.hpp"
#include "GraphicsManager.hpp"

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

void DebugManager::DrawLine(const Line& line, const vec4f& color, const std::chrono::seconds duration, bool depthEnabled) {
    AddPrimitive(std::make_unique<Line>(line), color, duration, depthEnabled);
}

void DebugManager::AddPrimitive(std::unique_ptr<Geometry> geometry, const vec4f& color, std::chrono::seconds duration, bool depthEnabled) {
    m_DebugPrimitives.emplace_back(std::move(geometry), color, std::chrono::high_resolution_clock::now() + duration);
    // make min heap
    std::push_heap(m_DebugPrimitives.begin(), m_DebugPrimitives.end(), cmp);
}

}  // namespace Hitagi::Debugger