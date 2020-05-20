#pragma once
#include <queue>
#include <variant>

#include <glad/glad.h>

#include "GraphicsManager.hpp"

namespace Hitagi::Graphics::OpenGL {

class OpenGLGraphicsManager : public GraphicsManager {
public:
    int  Initialize() final;
    void Tick() final;
    void Finalize() final;
    void Clear() final;
    void Draw() final;

    void RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color) final;

#if defined(_DEBUG)
    void DrawDebugMesh(const Resource::SceneObjectMesh& mesh, const mat4f& transform) final {}
    void ClearDebugBuffers() final;
#endif

private:
    using TshaderParameter = std::variant<float, vec2f, vec3f, vec4f, mat4f, GLuint>;
    bool SetShaderParameters(GLuint shader, const char* paramName, TshaderParameter param);
    bool UpdateFrameParameters(GLuint shader);

    void InitializeBuffers(const Resource::Scene& scene) final;
    void ClearBuffers() final;
    void ClearShaders() final;
    bool InitializeShaders() final;
    void RenderBuffers() final;

    struct MeshBuffer {
        GLuint                                       vao;
        std::vector<GLuint>                          vbos;
        GLuint                                       ebo;
        GLenum                                       mode;
        GLenum                                       type;
        size_t                                       indexCount;
        std::weak_ptr<Resource::SceneObjectMaterial> material;
    };

    struct DrawBatchContext {
        std::weak_ptr<Resource::SceneGeometryNode> node;
        std::shared_ptr<MeshBuffer>                mesh;
    };
    struct ShaderInfo {
        std::string path;
        int         type;
        GLuint      shaderId;
    };
    struct ShaderProgram {
        GLuint                   programId;
        std::vector<ShaderInfo>  shaders;
        std::vector<std::string> layout;
    };
    struct CharacterInfo {
        GLuint              textureID;
        Vector<int, 2>      bearing;
        Vector<unsigned, 2> size;
        int                 advance;
    };

    struct TextRenderInfo {
        std::string text;
        vec2f       position;
        vec3f       color;
    };

    ShaderProgram m_BasicShader = {0,
                                   {
                                       {"Asset/Shaders/basic.vs", GL_VERTEX_SHADER, 0},
                                       {"Asset/Shaders/basic.fs", GL_FRAGMENT_SHADER, 0},
                                   },
                                   {"POSITION", "NORMAL", "TEXCOORD"}};

    ShaderProgram m_TextShader = {0,
                                  {
                                      {"Asset/Shaders/text.vs", GL_VERTEX_SHADER, 0},
                                      {"Asset/Shaders/text.fs", GL_FRAGMENT_SHADER, 0},
                                  },
                                  {"vertex"}};

    std::unordered_map<xg::Guid, GLuint>                      m_Textures;
    std::unordered_map<xg::Guid, std::shared_ptr<MeshBuffer>> m_MeshBuffers;
    std::vector<DrawBatchContext>                             m_DrawBatchContext;

    std::unordered_map<char, CharacterInfo> m_Characters;
    GLuint                                  m_TextRenderVAO = 0;
    GLuint                                  m_TextRenderVBO = 0;

    std::queue<TextRenderInfo> m_textRenderQueue;

#if defined(_DEBUG)
    struct DebugDrawBatchContext {
        GLuint  vao;
        GLenum  mode;
        GLsizei count;
        vec3f   color;
    };
    std::vector<DebugDrawBatchContext> m_DebugDrawBatchContext;
    std::vector<GLuint>                m_DebugBuffers;
    ShaderProgram                      m_DebugShader = {0,
                                   {
                                       {"Asset/Shaders/debug.vs", GL_VERTEX_SHADER, 0},
                                       {"Asset/Shaders/debug.fs", GL_FRAGMENT_SHADER, 0},
                                   },
                                   {"POSITION"}};
#endif
};
}  // namespace Hitagi::Graphics::OpenGL