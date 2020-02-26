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
    auto fontFacePath = g_App->GetConfiguration().fontFace;
    m_FontFaceFile    = g_AssetLoader->SyncOpenAndReadBinary(fontFacePath);
    if (result = FT_Init_FreeType(&m_FTLibrary); result)
        std::cerr << "[GraphicsManager] FreeType library initialize failed." << std::endl;
    if (result = FT_New_Memory_Face(m_FTLibrary, m_FontFaceFile.GetData(), m_FontFaceFile.GetDataSize(), 0, &m_FTFace);
        result)
        std::cerr << "[GraphicsManager] FreeType font face initialize Failed." << std::endl;
    if (result = FT_Set_Pixel_Sizes(m_FTFace, 16, 0); result)
        std::cerr << "[GraphicsManager] FreeType set size failed." << std::endl;

    return result;
}

void GraphicsManager::Finalize() {
#if defined(_DEBUG)
    ClearDebugBuffers();
#endif
    ClearBuffers();
    ClearShaders();

    // Release Free Type
    FT_Done_Face(m_FTFace);
    FT_Done_FreeType(m_FTLibrary);
}

void GraphicsManager::Tick() {
    if (g_SceneManager->IsSceneChanged()) {
        std::cout << "Detected Scene Change, reinitialize buffers ..." << std::endl;
        ClearBuffers();
        ClearShaders();
        const Scene& scene = g_SceneManager->GetSceneForRendering();
        InitializeShaders();
        InitializeBuffers(scene);
        g_SceneManager->NotifySceneIsRenderingQueued();
    }
    g_GraphicsManager->RenderText("123123gaasdwqzxcx", vec2f(50, 50), 1, vec3f(1.0f));
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
    m_FrameConstants.worldMatrix   = mat4f(1.0f);
    m_FrameConstants.lightPosition = vec3f(2, 2, 2);
}

bool GraphicsManager::InitializeShaders() {
    std::cout << "[RHI] GraphicsManager::InitializeShader()" << std::endl;

    return true;
}

void GraphicsManager::ClearShaders() { std::cout << "[GraphicsManager] GraphicsManager::ClearShaders()" << std::endl; }

void GraphicsManager::CalculateCameraMatrix() {
    auto& scene      = g_SceneManager->GetSceneForRendering();
    auto  cameraNode = scene.GetFirstCameraNode();

    mat4f& viewMat = m_FrameConstants.viewMatrix;
    if (cameraNode) {
        viewMat = *cameraNode->GetCalculatedTransform();
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

    if (cameraNode) {
        auto camera      = scene.GetCamera(cameraNode->GetSceneObjectRef());
        fieldOfView      = std::dynamic_pointer_cast<SceneObjectPerspectiveCamera>(camera)->GetFov();
        nearClipDistance = camera->GetNearClipDistance();
        farClipDistance  = camera->GetFarClipDistance();
    }
    const GfxConfiguration& conf         = g_App->GetConfiguration();
    float                   screenAspect = (float)conf.screenWidth / (float)conf.screenHeight;
    m_FrameConstants.projectionMatrix    = perspective(fieldOfView, screenAspect, nearClipDistance, farClipDistance);

    m_FrameConstants.WVP =
        m_FrameConstants.projectionMatrix * m_FrameConstants.viewMatrix * m_FrameConstants.worldMatrix;
}

void GraphicsManager::CalculateLights() {
    auto& scene = g_SceneManager->GetSceneForRendering();

    vec3f& lightPos   = m_FrameConstants.lightPosition;
    vec4f& lightColor = m_FrameConstants.lightColor;

    if (auto lightNode = scene.GetFirstLightNode()) {
        lightPos       = vec3f(0.0f);
        auto _lightPos = (*lightNode->GetCalculatedTransform()) * vec4f(lightPos, 1.0f);
        lightPos       = vec3f(_lightPos.xyz);

        if (auto pLight = scene.GetLight(lightNode->GetSceneObjectRef())) {
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
    auto index = FT_Get_Char_Index(m_FTFace, c);
    FT_Load_Glyph(m_FTFace, index, FT_LOAD_DEFAULT);
    FT_Render_Glyph(m_FTFace->glyph, FT_RENDER_MODE_NORMAL);
    auto& glyph = m_FTFace->glyph;
    return glyph;
}

#if defined(_DEBUG)
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