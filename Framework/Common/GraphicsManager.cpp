#include <iostream>
#include "BaseApplication.hpp"
#include "GraphicsManager.hpp"
#include "SceneManager.hpp"
#include "AssetLoader.hpp"

using namespace My;

int GraphicsManager::Initialize() {
    int result = 0;
    InitConstants();

    // Initialize Free Type
    auto fontFacePath = g_pApp->GetConfiguration().fontFace;
    m_fontFaceFile    = g_pAssetLoader->SyncOpenAndReadBinary(fontFacePath);
    if (result = FT_Init_FreeType(&m_ftLibrary); result)
        std::cerr << "[GraphicsManager] FreeType library initialize failed." << std::endl;
    if (result = FT_New_Memory_Face(m_ftLibrary, m_fontFaceFile.GetData(), m_fontFaceFile.GetDataSize(), 0, &m_ftFace);
        result)
        std::cerr << "[GraphicsManager] FreeType font face initialize Failed." << std::endl;
    if (result = FT_Set_Pixel_Sizes(m_ftFace, 16, 0); result)
        std::cerr << "[GraphicsManager] FreeType set size failed." << std::endl;

    return result;
}

void GraphicsManager::Finalize() {
#ifdef DEBUG
    ClearDebugBuffers();
#endif
    ClearBuffers();
    ClearShaders();

    // Release Free Type
    FT_Done_Face(m_ftFace);
    FT_Done_FreeType(m_ftLibrary);
}

void GraphicsManager::Tick() {
    if (g_pSceneManager->IsSceneChanged()) {
        std::cout << "Detected Scene Change, reinitialize buffers ..." << std::endl;
        ClearBuffers();
        ClearShaders();
        const Scene& scene = g_pSceneManager->GetSceneForRendering();
        InitializeShaders();
        InitializeBuffers(scene);
        g_pSceneManager->NotifySceneIsRenderingQueued();
    }
    g_pGraphicsManager->RenderText("123123gaasdwqzxcx", vec2f(50, 50), 1, vec3f(1.0f));
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
    m_frameConstants.worldMatrix   = mat4f(1.0f);
    m_frameConstants.lightPosition = vec3f(2, 2, 2);
}

bool GraphicsManager::InitializeShaders() {
    std::cout << "[RHI] GraphicsManager::InitializeShader()" << std::endl;

    return true;
}

void GraphicsManager::ClearShaders() { std::cout << "[GraphicsManager] GraphicsManager::ClearShaders()" << std::endl; }

void GraphicsManager::CalculateCameraMatrix() {
    auto& scene       = g_pSceneManager->GetSceneForRendering();
    auto  pCameraNode = scene.GetFirstCameraNode();

    mat4f& viewMat = m_frameConstants.viewMatrix;
    if (pCameraNode) {
        viewMat = *pCameraNode->GetCalculatedTransform();
        viewMat = inverse(viewMat);
    } else {
        vec3f position(3, 3, 3);
        vec3f look_at(0, 0, 0);
        vec3f up(-1, -1, 1);
        viewMat = lookAt(position, look_at, up);
    }

    float fieldOfView      = PI / 2.0f;
    float nearClipDistance = 0.1f;
    float farClipDistance  = 100.0f;

    if (pCameraNode) {
        auto pCamera     = scene.GetCamera(pCameraNode->GetSceneObjectRef());
        fieldOfView      = std::dynamic_pointer_cast<SceneObjectPerspectiveCamera>(pCamera)->GetFov();
        nearClipDistance = pCamera->GetNearClipDistance();
        farClipDistance  = pCamera->GetFarClipDistance();
    }
    const GfxConfiguration& conf         = g_pApp->GetConfiguration();
    float                   screenAspect = (float)conf.screenWidth / (float)conf.screenHeight;
    m_frameConstants.projectionMatrix    = perspective(fieldOfView, screenAspect, nearClipDistance, farClipDistance);

    m_frameConstants.WVP =
        m_frameConstants.projectionMatrix * m_frameConstants.viewMatrix * m_frameConstants.worldMatrix;
}

void GraphicsManager::CalculateLights() {
    auto& scene = g_pSceneManager->GetSceneForRendering();

    vec3f& lightPos   = m_frameConstants.lightPosition;
    vec4f& lightColor = m_frameConstants.lightColor;

    if (auto pLightNode = scene.GetFirstLightNode()) {
        lightPos       = vec3f(0.0f);
        auto _lightPos = (*pLightNode->GetCalculatedTransform()) * vec4f(lightPos, 1.0f);
        lightPos       = vec3f(_lightPos.xyz);

        if (auto pLight = scene.GetLight(pLightNode->GetSceneObjectRef())) {
            lightColor = pLight->GetColor().Value;
        }
    } else {
        auto _lightPos = rotateZ(mat4f(1.0f), radians(1.0f)) * vec4f(lightPos, 1.0f);
        lightPos       = _lightPos.xyz;
        lightColor     = vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
}
void GraphicsManager::InitializeBuffers(const Scene& scene) {
    std::cout << "[GraphicsManager] GraphicsManager::InitializeBuffers()" << std::endl;
}
void GraphicsManager::ClearBuffers() { std::cout << "[GraphicsManager] GraphicsManager::ClearBuffers()" << std::endl; }
void GraphicsManager::RenderBuffers() {
    std::cout << "[GraphicsManager] GraphicsManager::RenderBuffers()" << std::endl;
}

const FT_GlyphSlot GraphicsManager::GetGlyph(char c) {
    // Generate a bitmap of c.
    auto index = FT_Get_Char_Index(m_ftFace, c);
    FT_Load_Glyph(m_ftFace, index, FT_LOAD_DEFAULT);
    FT_Render_Glyph(m_ftFace->glyph, FT_RENDER_MODE_NORMAL);
    auto& glyph = m_ftFace->glyph;
    return glyph;
}

#ifdef DEBUG
void GraphicsManager::RenderLine(const vec3f& from, const vec3f& to, const vec3f& color) {
    std::cout << "[GraphicsManager] GraphicsManager::RenderLine(" << from << "," << to << "," << color << ")"
              << std::endl;
}

void GraphicsManager::RenderBox(const vec3f& bbMin, const vec3f& bbMax, const vec3f& color) {
    std::cout << "[GraphicsManager] GraphicsManager::RenderBox(" << bbMin << "," << bbMax << "," << color << ")"
              << std::endl;
}

void GraphicsManager::RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) {
    std::cout << "[GraphicsManager] GraphicsManager::RenderText [" << text << ']' << std::endl;
}

void GraphicsManager::ClearDebugBuffers() {
    std::cout << "[GraphicsManager] GraphicsManager::ClearDebugBuffers(void)" << std::endl;
}
#endif