#include <iostream>
#include "GraphicsManager.hpp"
#include "SceneManager.hpp"
#include "IApplication.hpp"

using namespace My;
using namespace std;

int GraphicsManager::Initialize() {
    int result = 0;
    InitConstants();
    return result;
}

void GraphicsManager::Finalize() {
#ifdef DEBUG
    ClearDebugBuffers();
#endif
    ClearBuffers();
    ClearShaders();
}

void GraphicsManager::Tick() {
    if (g_pSceneManager->IsSceneChanged()) {
        cout << "Detected Scene Change, reinitialize buffers ..." << endl;
        ClearBuffers();
        ClearShaders();
        const Scene& scene = g_pSceneManager->GetSceneForRendering();
        InitializeBuffers(scene);
        InitializeShaders();
        g_pSceneManager->NotifySceneIsRenderingQueued();
    }
    UpdateConstants();

    Clear();
    Draw();
}
void GraphicsManager::Draw() {
    UpdateConstants();
    RenderBuffers();
}
void GraphicsManager::Clear() {}

void GraphicsManager::UpdateConstants() {
    CalculateCameraMatrix();
    CalculateLights();
}

void GraphicsManager::InitConstants() {
    // Initialize the world/model matrix to the identity matrix.
    m_DrawFrameContext.m_worldMatrix = mat4(1.0f);
}

bool GraphicsManager::InitializeShaders() {
    cout << "[RHI] GraphicsManager::InitializeShader(const char* vsFilename, "
            "const char* fsFilename)"
         << endl;

    return true;
}

void GraphicsManager::ClearShaders() {
    cout << "[GraphicsManager] GraphicsManager::ClearShaders()" << endl;
}

void GraphicsManager::CalculateCameraMatrix() {
    auto& scene       = g_pSceneManager->GetSceneForRendering();
    auto  pCameraNode = scene.GetFirstCameraNode();

    mat4& viewMat = m_DrawFrameContext.m_viewMatrix;
    if (pCameraNode) {
        viewMat = *pCameraNode->GetCalculatedTransform();
        viewMat = inverse(viewMat);
    } else {
        vec3 position(0, -5, 0);
        vec3 look_at(0, 0, 0);
        vec3 up(0, 0, 1);
        viewMat = lookAt(position, look_at, up);
    }

    float fieldOfView      = PI / 2.0f;
    float nearClipDistance = 1.0f;
    float farClipDistance  = 100.0f;

    if (pCameraNode) {
        auto pCamera = scene.GetCamera(pCameraNode->GetSceneObjectRef());
        fieldOfView =
            dynamic_pointer_cast<SceneObjectPerspectiveCamera>(pCamera)
                ->GetFov();
        nearClipDistance = pCamera->GetNearClipDistance();
        farClipDistance  = pCamera->GetFarClipDistance();
    }
    const GfxConfiguration& conf = g_pApp->GetConfiguration();
    float screenAspect = (float)conf.screenWidth / (float)conf.screenHeight;
    m_DrawFrameContext.m_projectionMatrix = perspective(
        fieldOfView, screenAspect, nearClipDistance, farClipDistance);
}

void GraphicsManager::CalculateLights() {
    auto& scene      = g_pSceneManager->GetSceneForRendering();
    auto  pLightNode = scene.GetFirstLightNode();

    vec3& lightPos   = m_DrawFrameContext.m_lightPosition;
    vec4& lightColor = m_DrawFrameContext.m_lightColor;

    if (pLightNode) {
        lightPos = vec3(0.0f);
        auto _lightPos =
            vec4(lightPos, 1.0f) * (*pLightNode->GetCalculatedTransform());
        lightPos = vec3(_lightPos);

        auto pLight = scene.GetLight(pLightNode->GetSceneObjectRef());
        if (pLight) {
            lightColor = pLight->GetColor().Value;
        }
    } else {
        lightPos   = vec3(-1.0f, -5.0f, 0.0f);
        lightColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}
void GraphicsManager::InitializeBuffers(const Scene& scene) {
    cout << "[GraphicsManager] GraphicsManager::InitializeBuffers()" << endl;
}
void GraphicsManager::ClearBuffers() {
    cout << "[GraphicsManager] GraphicsManager::ClearBuffers()" << endl;
}
void GraphicsManager::RenderBuffers() {
    cout << "[RHI] GraphicsManager::RenderBuffers()" << endl;
}

#ifdef DEBUG
void GraphicsManager::DrawLine(const vec3& from, const vec3& to,
                               const vec3& color) {
    cout << "[GraphicsManager] GraphicsManager::DrawLine(" << from << "," << to
         << "," << color << ")" << endl;
}

void GraphicsManager::DrawBox(const vec3& bbMin, const vec3& bbMax,
                              const vec3& color) {
    cout << "[GraphicsManager] GraphicsManager::DrawBox(" << bbMin << ","
         << bbMax << "," << color << ")" << endl;
}

void GraphicsManager::ClearDebugBuffers() {
    cout << "[GraphicsManager] GraphicsManager::ClearDebugBuffers(void)"
         << endl;
}
#endif