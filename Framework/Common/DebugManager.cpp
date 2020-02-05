#include <iostream>
#include "DebugManager.hpp"
#include "GraphicsManager.hpp"
#include "IPhysicsManager.hpp"

using namespace My;

int DebugManager::Initialize() { return 0; }

void DebugManager::Finalize() {}

void DebugManager::Tick() {
#ifdef DEBUG
    if (m_bDrawDebugInfo) {
        g_pGraphicsManager->ClearDebugBuffers();
        DrawDebugInfo();
        g_pPhysicsManager->DrawDebugInfo();
    }
#endif
}

void DebugManager::ToggleDebugInfo() {
#ifdef DEBUG
    m_bDrawDebugInfo = !m_bDrawDebugInfo;
    if (!m_bDrawDebugInfo) g_pGraphicsManager->ClearDebugBuffers();
#endif
}

void DebugManager::DrawDebugInfo() {
#ifdef DEBUG
    // x - axis
    vec3f from(0.0f, 0.0f, 0.0f);
    vec3f to(10.0f, 0.0f, 0.0f);
    vec3f color(1.0f, 0.0f, 0.0f);
    g_pGraphicsManager->DrawLine(from, to, color);

    // y - axis
    from  = vec3f(0.0f, 0.0f, 0.0f);
    to    = vec3f(0.0f, 10.0f, 0.0f);
    color = vec3f(0.0f, 1.0f, 0.0f);
    g_pGraphicsManager->DrawLine(from, to, color);

    // z - axis
    from  = vec3f(0.0f, 0.0f, 0.0f);
    to    = vec3f(0.0f, 0.0f, 10.0f);
    color = vec3f(0.0f, 0.0f, 1.0f);
    g_pGraphicsManager->DrawLine(from, to, color);
#endif
}