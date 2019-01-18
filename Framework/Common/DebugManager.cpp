#include <iostream>
#include "DebugManager.hpp"
#include "GraphicsManager.hpp"
#include "IPhysicsManager.hpp"

using namespace My;
using namespace std;

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
    vec3 from(-1000.0f, 0.0f, 0.0f);
    vec3 to(1000.0f, 0.0f, 0.0f);
    vec3 color(1.0f, 0.0f, 0.0f);
    g_pGraphicsManager->DrawLine(from, to, color);

    // y - axis
    from  = vec3(0.0f, -1000.0f, 0.0f);
    to    = vec3(0.0f, 1000.0f, 0.0f);
    color = vec3(0.0f, 1.0f, 0.0f);
    g_pGraphicsManager->DrawLine(from, to, color);

    // z - axis
    from  = vec3(0.0f, 0.0f, -1000.0f);
    to    = vec3(0.0f, 0.0f, 1000.0f);
    color = vec3(0.0f, 0.0f, 1.0f);
    g_pGraphicsManager->DrawLine(from, to, color);
#endif
}