#pragma once
#include "IRuntimeModule.hpp"
#include "geommath.hpp"
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

#if defined(DEBUG)
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

    struct FrameConstants {
        mat4 WVP;
        mat4 worldMatrix;
        mat4 viewMatrix;
        mat4 projectionMatrix;
        vec3 lightPosition;
        vec4 lightColor;
    };

    struct DrawBatchContext {
        std::shared_ptr<SceneGeometryNode>   node;
        std::shared_ptr<SceneObjectMaterial> material;
    };

    FrameConstants m_FrameConstants;
};

extern std::unique_ptr<GraphicsManager> g_pGraphicsManager;

}  // namespace My
