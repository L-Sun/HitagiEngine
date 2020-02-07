#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <glad/glad.h>

#include "GraphicsManager.hpp"
#include "SceneObject.hpp"
#include "SceneNode.hpp"
#include "geommath.hpp"

namespace My {
class OpenGLGraphicsManager : public GraphicsManager {
public:
    int  Initialize() final;
    void Finalize() final;
    void Clear() final;
    void Draw() final;

#ifdef DEBUG
    void DrawLine(const vec3f& from, const vec3f& to, const vec3f& color) final;
    void DrawBox(const vec3f& bbMin, const vec3f& bbMax, const vec3f& color) final;
    void ClearDebugBuffers() final;
#endif

private:
    bool SetPerBatchShaderParameters(GLuint shader, const char* paramName,
                                     const mat4f& param);
    bool SetPerBatchShaderParameters(GLuint shader, const char* paramName,
                                     const vec3f& param);
    bool SetPerBatchShaderParameters(GLuint shader, const char* paramName,
                                     const float param);
    bool SetPerBatchShaderParameters(GLuint shader, const char* paramName,
                                     const GLint texture_index);
    bool SetPerFrameShaderParameters(GLuint shader);

    void InitializeBuffers(const Scene& scene) final;
    void ClearBuffers() final;
    void ClearShaders() final;
    bool InitializeShaders() final;
    void RenderBuffers() final;

    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    GLuint m_shaderProgram;

#ifdef DEBUG
    GLuint m_debugVertexShader;
    GLuint m_debugFragmentShader;
    GLuint m_debugShaderProgram;
#endif

    std::map<std::string, GLint> m_TextureIndex;

    struct OpenGLDrawBatchContext : public DrawBatchContext {
        size_t count;  // index count per node
        GLuint vao;
        GLuint ebo;
        GLenum mode;
        GLenum type;
    };

#ifdef DEBUG
    struct DebugDrawBatchContext {
        GLuint  vao;
        GLenum  mode;
        GLsizei count;
        vec3f   color;
    };
#endif

    std::vector<OpenGLDrawBatchContext> m_DrawBatchContext;
    std::vector<GLuint>                 m_Buffers;
    std::vector<GLuint>                 m_Textures;

#ifdef DEBUG
    std::vector<DebugDrawBatchContext> m_DebugDrawBatchContext;
    std::vector<GLuint>                m_DebugBuffers;
#endif
};
}  // namespace My