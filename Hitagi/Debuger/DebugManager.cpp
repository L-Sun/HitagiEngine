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
#if defined(_DEBUG)
    if (m_DrawDebugInfo) {
        g_GraphicsManager->ClearDebugBuffers();
        DrawDebugInfo();
        g_PhysicsManager->DrawDebugInfo();
    }
#endif
}

void DebugManager::ToggleDebugInfo() {
#if defined(_DEBUG)
    m_DrawDebugInfo = !m_DrawDebugInfo;
    if (!m_DrawDebugInfo) g_GraphicsManager->ClearDebugBuffers();
#endif
}

void DebugManager::DrawDebugInfo() {
#if defined(_DEBUG)
    // x - axis
    vec3f from(0.0f, 0.0f, 0.0f);
    vec3f to(10.0f, 0.0f, 0.0f);
    vec3f color(1.0f, 0.0f, 0.0f);
    g_GraphicsManager->RenderLine(from, to, color);

    // y - axis
    from  = vec3f(0.0f, 0.0f, 0.0f);
    to    = vec3f(0.0f, 10.0f, 0.0f);
    color = vec3f(0.0f, 1.0f, 0.0f);
    g_GraphicsManager->RenderLine(from, to, color);

    // z - axis
    from  = vec3f(0.0f, 0.0f, 0.0f);
    to    = vec3f(0.0f, 0.0f, 10.0f);
    color = vec3f(0.0f, 0.0f, 1.0f);
    g_GraphicsManager->RenderLine(from, to, color);
#endif
}
}  // namespace Hitagi