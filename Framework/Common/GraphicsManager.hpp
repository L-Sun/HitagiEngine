#pragma once
#include "IRuntimeModule.hpp"
#include "geommath.hpp"
#include "Image.hpp"

namespace My {
class GraphicsManager : implements IRuntimeModule {
public:
    virtual ~GraphicsManager() {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();
    virtual void Draw();
    virtual void Clear();

#ifdef DEBUG
    virtual void DrawLine(const vec3& from, const vec3& to, const vec3& color);
    virtual void DrawBox(const vec3& bbMin, const vec3& bbMax,
                         const vec3& color);
    virtual void ClearDebugBuffers();
#endif

    void WorldRotateX(float angle);
    void WorldRotateY(float angle);

protected:
    bool SetPerFrameShaderParameters();
    bool SetPerBatchShaderParameters(const char* paramName, const mat4& param);
    bool SetPerBatchShaderParameters(const char* paramName, const vec3& param);
    bool SetPerBatchShaderParameters(const char* paramName, const float param);
    bool SetPerBatchShaderParameters(const char* paramName, const int param);

    void InitConstants();
    bool InitializeShader(const char* vsFilename, const char* fsFilename);
    void InitializeBuffers();
    void CalculateCameraMatrix();
    void CalculateLights();
    void RenderBuffers();

    struct DrawFrameContext {
        mat4 m_worldMatrix;
        mat4 m_viewMatrix;
        mat4 m_projectionMatrix;
        vec3 m_lightPosition;
        vec4 m_lightColor;
    };

    DrawFrameContext m_DrawFrameContext;
};

extern GraphicsManager* g_pGraphicsManager;
}  // namespace My
