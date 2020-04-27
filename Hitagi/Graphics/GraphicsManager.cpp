#include "GraphicsManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "IApplication.hpp"
#include "SceneManager.hpp"

namespace Hitagi::Graphics {

int GraphicsManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GraphicsManager");
    m_Logger->info("Initialize...");

    int result = m_ShaderManager.Initialize();
    InitConstants();

    // Initialize Free Type
    auto fontFacePath = g_App->GetConfiguration().fontFace;
    m_FontFaceFile    = g_FileIOManager->SyncOpenAndReadBinary(fontFacePath);
    if (result = FT_Init_FreeType(&m_FTLibrary); result) {
        m_Logger->error("FreeType library initialize failed.");
    }
    if (result = FT_New_Memory_Face(m_FTLibrary, m_FontFaceFile.GetData(), m_FontFaceFile.GetDataSize(), 0, &m_FTFace);
        result) {
        m_Logger->error("FreeType font face initialize Failed.");
    }
    if (result = FT_Set_Pixel_Sizes(m_FTFace, 16, 0); result) {
        m_Logger->error("FreeType set size failed.");
    }

    return result;
}

void GraphicsManager::Finalize() {
#if defined(_DEBUG)
    ClearDebugBuffers();
#endif
    ClearBuffers();
    ClearShaders();

    m_ShaderManager.Finalize();
    // Release Free Type
    FT_Done_Face(m_FTFace);
    FT_Done_FreeType(m_FTLibrary);

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void GraphicsManager::Tick() {
    if (g_SceneManager->IsSceneChanged()) {
        m_Logger->info("Detected Scene Change, reinitialize buffers ...");
        ClearBuffers();
        ClearShaders();
        const Resource::Scene& scene = g_SceneManager->GetSceneForRendering();
        InitializeShaders();
        InitializeBuffers(scene);
        g_SceneManager->NotifySceneIsRenderingQueued();
    }
    g_GraphicsManager->RenderText("A fox jumped over the lazy brown dog.", vec2f(50, 50), 1, vec3f(1.0f));
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
    m_Logger->debug("Initialize shaders.");
    return true;
}

void GraphicsManager::ClearShaders() { m_Logger->debug("clear shaders."); }

void GraphicsManager::CalculateCameraMatrix() {
    auto& scene      = g_SceneManager->GetSceneForRendering();
    auto  cameraNode = scene.GetFirstCameraNode();

    mat4f& viewMat = m_FrameConstants.viewMatrix;
    viewMat        = cameraNode->GetViewMatrix();

    auto  camera           = scene.GetCamera(cameraNode->GetSceneObjectRef());
    float fieldOfView      = camera->GetFov();
    float nearClipDistance = camera->GetNearClipDistance();
    float farClipDistance  = camera->GetFarClipDistance();

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
        auto _lightPos = lightNode->GetCalculatedTransform() * vec4f(lightPos, 1.0f);
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
void GraphicsManager::InitializeBuffers(const Resource::Scene& scene) { m_Logger->debug("Initialize buffers."); }
void GraphicsManager::ClearBuffers() { m_Logger->debug("Clear buffers."); }
void GraphicsManager::RenderBuffers() { m_Logger->debug("Render buffers."); }

const FT_GlyphSlot GraphicsManager::GetGlyph(char c) {
    // Generate a bitmap of c.
    auto index = FT_Get_Char_Index(m_FTFace, c);
    FT_Load_Glyph(m_FTFace, index, FT_LOAD_DEFAULT);
    FT_Render_Glyph(m_FTFace->glyph, FT_RENDER_MODE_NORMAL);
    auto& glyph = m_FTFace->glyph;
    return glyph;
}

void GraphicsManager::RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) {
    m_Logger->debug("Render text: {}, position: ({}, {}), scale: {}, color: ({}, {}, {})", text, position.x, position.y,
                    scale, color.r, color.g, color.b);
}

#if defined(_DEBUG)
void GraphicsManager::RenderLine(const vec3f& from, const vec3f& to, const vec3f& color) {
    m_Logger->debug("Draw line from ({}, {}, {}) to ({}, {}, {})", from.x, from.y, from.z, to.x, to.y, to.z);
}

void GraphicsManager::RenderBox(const vec3f& bbMin, const vec3f& bbMax, const vec3f& color) {
    m_Logger->debug("Draw box bbMin:({}, {}, {}), bbMax: ({}, {}, {}), color: ({}, {}, {})", bbMin.x, bbMin.y, bbMin.z,
                    bbMax.x, bbMax.y, bbMax.z, color.r, color.g, color.b);
}
void GraphicsManager::ClearDebugBuffers() { m_Logger->debug("Clear debug buffers"); }
#endif  // _DEBUG

}  // namespace Hitagi::Graphics