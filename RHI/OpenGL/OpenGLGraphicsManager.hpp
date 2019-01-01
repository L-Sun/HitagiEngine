#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <map>

#include "GraphicsManager.hpp"
#include "SceneObject.hpp"
#include "geommath.hpp"
#include "glad/glad.h"

namespace My {
class OpenGLGraphicsManager : public GraphicsManager {
public:
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Clear();
    virtual void Draw();
    virtual void Tick();

private:
    bool SetPerBatchShaderParameters(const char* paramName, const mat4& param);
    bool SetPerBatchShaderParameters(const char* paramName, const vec3& param);
    bool SetPerBatchShaderParameters(const char* paramName, const float param);
    bool SetPerBatchShaderParameters(const char* paramName,
                                     const GLint texture_index);
    bool SetPerFrameShaderParameters();

    void InitializeBuffers();
    void RenderBuffers();
    void CalculateCameraMatrix();
    void CalculateLights();
    bool InitializeShader(const char* vsFilename, const char* fsFilename);

    unsigned int                 m_vertexShader;
    unsigned int                 m_fragmentShader;
    unsigned int                 m_shaderProgram;
    std::map<std::string, GLint> m_TextureIndex;

    struct DrawFrameContext {
        mat4 m_worldMatrix;
        mat4 m_viewMatrix;
        mat4 m_projectionMatrix;
        vec3 m_lightPosition;
        vec4 m_lightColor;
    };

    struct DrawBatchContext {
        GLuint                               vao;
        GLenum                               mode;
        GLenum                               type;
        GLsizei                              count;
        std::shared_ptr<mat4>                transform;
        std::shared_ptr<SceneObjectMaterial> material;
    };

    DrawFrameContext m_DrawFrameContext;

    std::vector<DrawBatchContext> m_DrawBatchContext;
    std::vector<GLuint>           m_Buffers;
    std::vector<GLuint>           m_Textures;
};
}  // namespace My