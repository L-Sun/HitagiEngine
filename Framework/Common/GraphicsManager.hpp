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
    virtual void DrawLine(const vec3f& from, const vec3f& to, const vec3f& color);
    virtual void DrawBox(const vec3f& bbMin, const vec3f& bbMax,
                         const vec3f& color);
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
        mat4f WVP;
        mat4f worldMatrix;
        mat4f viewMatrix;
        mat4f projectionMatrix;
        vec3f lightPosition;
        vec4f lightColor;
    };

    struct DrawBatchContext {
        std::shared_ptr<SceneGeometryNode>   node;
        std::shared_ptr<SceneObjectMaterial> material;
    };

    FrameConstants m_FrameConstants;
};

extern std::unique_ptr<GraphicsManager> g_pGraphicsManager;

}  // namespace My
