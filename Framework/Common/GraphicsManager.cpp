#include <iostream>
#include "GraphicsManager.hpp"
#include "SceneManager.hpp"
#include "IApplication.hpp"

using namespace My;

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
        std::cout << "Detected Scene Change, reinitialize buffers ..."
                  << std::endl;
        ClearBuffers();
        ClearShaders();
        const Scene& scene = g_pSceneManager->GetSceneForRendering();
        InitializeShaders();
        InitializeBuffers(scene);
        g_pSceneManager->NotifySceneIsRenderingQueued();
    }

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
    m_FrameConstants.worldMatrix   = mat4(1.0f);
    m_FrameConstants.lightPosition = vec3(5, 5, 5);
}

bool GraphicsManager::InitializeShaders() {
    std::cout << "[RHI] GraphicsManager::InitializeShader()" << std::endl;

    return true;
}

void GraphicsManager::ClearShaders() {
    std::cout << "[GraphicsManager] GraphicsManager::ClearShaders()"
              << std::endl;
}

void GraphicsManager::CalculateCameraMatrix() {
    auto& scene       = g_pSceneManager->GetSceneForRendering();
    auto  pCameraNode = scene.GetFirstCameraNode();

    mat4& viewMat = m_FrameConstants.viewMatrix;
    if (pCameraNode) {
        viewMat = *pCameraNode->GetCalculatedTransform();
        viewMat = inverse(viewMat);
    } else {
        vec3 position(5, 5, 5);
        vec3 look_at(0, 0, 0);
        vec3 up(-1, -1, 1);
        viewMat = lookAt(position, look_at, up);
    }

    float fieldOfView      = PI / 2.0f;
    float nearClipDistance = 1.0f;
    float farClipDistance  = 100.0f;

    if (pCameraNode) {
        auto pCamera = scene.GetCamera(pCameraNode->GetSceneObjectRef());
        fieldOfView =
            std::dynamic_pointer_cast<SceneObjectPerspectiveCamera>(pCamera)
                ->GetFov();
        nearClipDistance = pCamera->GetNearClipDistance();
        farClipDistance  = pCamera->GetFarClipDistance();
    }
    const GfxConfiguration& conf = g_pApp->GetConfiguration();
    float screenAspect = (float)conf.screenWidth / (float)conf.screenHeight;
    m_FrameConstants.projectionMatrix = perspective(
        fieldOfView, screenAspect, nearClipDistance, farClipDistance);

    m_FrameConstants.WVP = m_FrameConstants.projectionMatrix *
                           m_FrameConstants.viewMatrix *
                           m_FrameConstants.worldMatrix;
}

void GraphicsManager::CalculateLights() {
    auto& scene = g_pSceneManager->GetSceneForRendering();

    vec3& lightPos   = m_FrameConstants.lightPosition;
    vec4& lightColor = m_FrameConstants.lightColor;

    if (auto pLightNode = scene.GetFirstLightNode()) {
        lightPos = vec3(0.0f);
        auto _lightPos =
            (*pLightNode->GetCalculatedTransform()) * vec4(lightPos, 1.0f);
        lightPos = vec3(_lightPos.xyz);

        if (auto pLight = scene.GetLight(pLightNode->GetSceneObjectRef())) {
            lightColor = pLight->GetColor().Value;
        }
    } else {
        auto _lightPos =
            rotateZ(mat4(1.0f), radians(1.0f)) * vec4(lightPos, 1.0f);
        lightPos   = _lightPos.xyz;
        lightColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}
void GraphicsManager::InitializeBuffers(const Scene& scene) {
    std::cout << "[GraphicsManager] GraphicsManager::InitializeBuffers()"
              << std::endl;
}
void GraphicsManager::ClearBuffers() {
    std::cout << "[GraphicsManager] GraphicsManager::ClearBuffers()"
              << std::endl;
}
void GraphicsManager::RenderBuffers() {
    std::cout << "[RHI] GraphicsManager::RenderBuffers()" << std::endl;
}

#ifdef DEBUG
void GraphicsManager::DrawLine(const vec3& from, const vec3& to,
                               const vec3& color) {
    std::cout << "[GraphicsManager] GraphicsManager::DrawLine(" << from << ","
              << to << "," << color << ")" << std::endl;
}

void GraphicsManager::DrawBox(const vec3& bbMin, const vec3& bbMax,
                              const vec3& color) {
    std::cout << "[GraphicsManager] GraphicsManager::DrawBox(" << bbMin << ","
              << bbMax << "," << color << ")" << std::endl;
}

void GraphicsManager::ClearDebugBuffers() {
    std::cout << "[GraphicsManager] GraphicsManager::ClearDebugBuffers(void)"
              << std::endl;
}
#endif