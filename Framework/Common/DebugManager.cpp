#include <iostream>
#include "DebugManager.hpp"
#include "GraphicsManager.hpp"
#include "IPhysicsManager.hpp"

using namespace Hitagi;

int DebugManager::Initialize() { return 0; }

void DebugManager::Finalize() {}

void DebugManager::Tick() {
#if defined(_DEBUG)
    if (m_DrawDebugInfo) {
        g_GraphicsManager->ClearDebugBuffers();
        DrawDebugInfo();
        g_hysicsManager->DrawDebugInfo();
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