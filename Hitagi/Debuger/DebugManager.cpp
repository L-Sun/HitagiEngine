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
#if defined(_DEBUG)

#endif
}

void DebugManager::DrawDebugInfo() {
#if defined(_DEBUG)
    // // x - axis
    // g_GraphicsManager->RenderLine(Line{vec3f(0, 0, 0), vec3f(1000, 0, 0)}, vec4f(1, 0, 0, 1));
    // // y - axis
    // g_GraphicsManager->RenderLine(Line{vec3f(0, 0, 0), vec3f(0, 1000, 0)}, vec4f(0, 1, 0, 1));
    // // z - axis
    // g_GraphicsManager->RenderLine(Line{vec3f(0, 0, 0), vec3f(0, 0, 1000)}, vec4f(0, 0, 1, 1));

    // g_GraphicsManager->RenderGrid();
#endif
}
}  // namespace Hitagi