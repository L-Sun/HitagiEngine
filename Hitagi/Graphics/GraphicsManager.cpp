#include "GraphicsManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <numeric>

#include "IApplication.hpp"
#include "SceneManager.hpp"

namespace Hitagi::Graphics {

int GraphicsManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("GraphicsManager");
    m_Logger->info("Initialize...");

#if defined(_DEBUG)
    m_Logger->set_level(spdlog::level::debug);
#endif  // _DEBUG

    int result = m_ShaderManager.Initialize();
    if (result != 0) return result;
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
}

bool GraphicsManager::InitializeShaders() {
    m_Logger->debug("Initialize shaders.");
    return true;
}

void GraphicsManager::ClearShaders() { m_Logger->debug("clear shaders."); }

void GraphicsManager::CalculateCameraMatrix() {
    auto& scene = g_SceneManager->GetSceneForRendering();

    if (auto cameraNode = scene.GetFirstCameraNode(); cameraNode->Dirty()) {
        m_FrameConstants.view      = cameraNode->GetViewMatrix();
        m_FrameConstants.cameraPos = vec4f(cameraNode->GetCameraPosition(), 1.0f);

        float fieldOfView      = PI / 2.0f;
        float nearClipDistance = 0.f;
        float farClipDistance  = 100.0f;

        if (auto camera = cameraNode->GetSceneObjectRef().lock()) {
            fieldOfView      = camera->GetFov();
            nearClipDistance = camera->GetNearClipDistance();
            farClipDistance  = camera->GetFarClipDistance();
        }

        const GfxConfiguration& conf         = g_App->GetConfiguration();
        float                   screenAspect = (float)conf.screenWidth / (float)conf.screenHeight;
        m_FrameConstants.projection          = perspective(fieldOfView, screenAspect, nearClipDistance, farClipDistance);

        m_FrameConstants.projView      = m_FrameConstants.projection * m_FrameConstants.view;
        m_FrameConstants.invView       = inverse(m_FrameConstants.view);
        m_FrameConstants.invProjection = inverse(m_FrameConstants.projection);
        m_FrameConstants.invProjView   = inverse(m_FrameConstants.projView);

        cameraNode->ClearDirty();
    }
}

void GraphicsManager::CalculateLights() {
    auto& scene = g_SceneManager->GetSceneForRendering();

    auto lightNode                  = scene.GetFirstLightNode();
    m_FrameConstants.lightPosition  = vec4f(GetOrigin(lightNode->GetCalculatedTransform()), 1);
    m_FrameConstants.lightPosInView = m_FrameConstants.view * m_FrameConstants.lightPosition;
    if (auto light = lightNode->GetSceneObjectRef().lock()) {
        m_FrameConstants.lightIntensity = light->GetIntensity() * light->GetColor().Value;
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
    m_Logger->debug("Render text: {}, position: {}, scale: {}, color: {}", text, position, scale, color);
}

#if defined(_DEBUG)
Resource::SceneObjectMesh GenerateDebugMesh(std::vector<vec3f> vertices, std::vector<uint32_t> indices, Resource::PrimitiveType primitiveType) {
    Resource::SceneObjectMesh mesh;
    mesh.AddVertexArray(Resource::SceneObjectVertexArray("POSITION", Resource::VertexDataType::FLOAT3, vertices.data(), vertices.size()));
    mesh.AddIndexArray(Resource::SceneObjectIndexArray(Resource::IndexDataType::INT32, indices.data(), indices.size()));
    mesh.SetPrimitiveType(primitiveType);
    return mesh;
}

void GraphicsManager::RenderLine(const Line& line, const vec4f& color) {
    m_Logger->debug("Draw line from: {}, to: {}, color: {}", line.from, line.to, color);
    const std::vector<vec3f>    vertices = {line.from, line.to};
    const std::vector<uint32_t> indices  = {0, 1};
    DrawDebugMesh(GenerateDebugMesh(vertices, indices, Resource::PrimitiveType::LINE_LIST), mat4f(1.0f), color);
}

void GraphicsManager::RenderBox(const Box& box, const vec4f& color) {
    m_Logger->debug("Draw box bbMin: {}, bbMax: {}, color: {}", box.bbMin, box.bbMax, color);
    const std::vector<vec3f> vertices = {
        {box.bbMin.x, box.bbMin.y, box.bbMin.z},
        {box.bbMax.x, box.bbMin.y, box.bbMin.z},
        {box.bbMax.x, box.bbMax.y, box.bbMin.z},
        {box.bbMin.x, box.bbMax.y, box.bbMin.z},
        {box.bbMin.x, box.bbMin.y, box.bbMax.z},
        {box.bbMax.x, box.bbMin.y, box.bbMax.z},
        {box.bbMax.x, box.bbMax.y, box.bbMax.z},
        {box.bbMin.x, box.bbMax.y, box.bbMax.z},
    };
    const std::vector<uint32_t> indices = {
        0, 1, 1, 2, 2, 3, 3, 0,  // bottom
        4, 5, 5, 6, 6, 7, 7, 4,  // top
        0, 4, 1, 5, 2, 6, 3, 7,  // surround
    };
    DrawDebugMesh(GenerateDebugMesh(vertices, indices, Resource::PrimitiveType::LINE_LIST), mat4f(1.0f), color);
}

void GraphicsManager::RenderGrid(float spacing, const vec4f& color) {
    m_Logger->debug("Draw Grid(100x100) in xOy plane");
    std::vector<vec3f>    vertices;
    std::vector<uint32_t> indices(2 * (101 + 101));
    vertices.reserve(2 * (101 + 101));

    Line xLine{vec3f{-50 * spacing, -50 * spacing, 0}, vec3f{50 * spacing, -50 * spacing, -0.001}};
    Line yLine{vec3f{-50 * spacing, -50 * spacing, 0}, vec3f{-50 * spacing, 50 * spacing, -0.001}};

    for (size_t i = 0; i < 101; i++) {
        vertices.push_back(xLine.from);
        vertices.push_back(xLine.to);
        vertices.push_back(yLine.from);
        vertices.push_back(yLine.to);
        xLine.from.y += spacing;
        xLine.to.y += spacing;
        yLine.from.x += spacing;
        yLine.to.x += spacing;
    }

    std::iota(indices.begin(), indices.end(), 0);
    DrawDebugMesh(GenerateDebugMesh(vertices, indices, Resource::PrimitiveType::LINE_LIST), mat4f(1.0f), color);
}

void GraphicsManager::ClearDebugBuffers() { m_Logger->debug("Clear debug buffers"); }
#endif  // _DEBUG

}  // namespace Hitagi::Graphics