#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

#include "GraphicsManager.hpp"
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
    bool SetPerBatchShaderParameters(const char* paramName, float* param);
    bool SetPerFrameShaderParameters();

    void InitializeBuffers();
    void RenderBuffers();
    void CalculateCameraMatrix();
    void CalculateLights();
    bool InitializeShader(const char* vsFilename, const char* fsFilename);

    unsigned int m_vertexShader;
    unsigned int m_fragmentShader;
    unsigned int m_shaderProgram;

    struct DrawFrameContext {
        mat4 m_worldMatrix;
        mat4 m_viewMatrix;
        mat4 m_projectionMatrix;
        vec3 m_lightPosition;
        vec4 m_lightColor;
    };

    struct DrawBatchContext {
        GLuint                vao;
        GLenum                mode;
        GLenum                type;
        std::vector<GLsizei>  counts;
        std::shared_ptr<mat4> transform;
    };

    DrawFrameContext m_DrawFrameContext;

    std::vector<DrawBatchContext> m_DrawBatchContext;
    std::vector<GLuint>           m_Buffers;
};
}  // namespace My