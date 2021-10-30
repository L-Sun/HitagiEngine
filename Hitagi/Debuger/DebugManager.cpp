#include "DebugManager.hpp"

#include <iostream>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "IPhysicsManager.hpp"
#include "GraphicsManager.hpp"

namespace Hitagi {
std::unique_ptr<DebugManager> g_DebugManager = std::make_unique<DebugManager>();

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
}

void DebugManager::ToggleDebugInfo() {
}

void DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, const std::chrono::duration<double> duration) {
}

}  // namespace Hitagi