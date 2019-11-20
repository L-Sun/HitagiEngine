#pragma once
#include "IRuntimeModule.hpp"
#include "geommath.hpp"
#include "Image.hpp"
#include "Scene.hpp"

namespace My {
class GraphicsManager : public IRuntimeModule {
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

protected:
    virtual void InitConstants();
    virtual void InitializeBuffers(const Scene& scene);
    virtual bool InitializeShaders();
    virtual void ClearShaders();
    virtual void ClearBuffers();
    virtual void CalculateCameraMatrix();
    virtual void CalculateLights();
    virtual void UpdateConstants();
    virtual void RenderBuffers();

    struct DrawFrameContext {
        mat4 m_worldMatrix;
        mat4 m_viewMatrix;
        mat4 m_projectionMatrix;
        vec3 m_lightPosition;
        vec4 m_lightColor;
    };

    struct DrawBatchContext {
        size_t                               count;
        std::shared_ptr<SceneGeometryNode>   node;
        std::shared_ptr<SceneObjectMaterial> material;
    };

    DrawFrameContext m_DrawFrameContext;
};

extern std::unique_ptr<GraphicsManager> g_pGraphicsManager;

}  // namespace My
