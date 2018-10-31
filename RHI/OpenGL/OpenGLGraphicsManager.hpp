#pragma once
#include "GraphicsManager.hpp"
#include "glad/glad.h"
#include "glm.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

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
        glm::mat4 m_worldMatrix;
        glm::mat4 m_viewMatrix;
        glm::mat4 m_projectionMatrix;
        glm::vec3 m_lightPosition;
        glm::vec4 m_lightColor;
    };

    struct DrawBatchContext {
        GLuint                     vao;
        GLenum                     mode;
        GLenum                     type;
        GLsizei                    count;
        std::shared_ptr<glm::mat4> transform;
    };

    DrawFrameContext m_DrawFrameContext;

    std::vector<DrawBatchContext>                 m_VAO;
    std::unordered_map<std::string, unsigned int> m_Buffers;
};
}  // namespace My