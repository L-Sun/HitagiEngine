#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H

#include "IRuntimeModule.hpp"
#include "Scene.hpp"
#include "ShaderManager.hpp"

namespace Hitagi::Graphics {
class GraphicsManager : public IRuntimeModule {
public:
    virtual ~GraphicsManager() {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();
    virtual void Draw();
    virtual void Clear();

    virtual void RenderText(std::string_view text, const vec2f& position, float scale, const vec3f& color);

#if defined(_DEBUG)
    virtual void RenderLine(const vec3f& from, const vec3f& to, const vec3f& color);
    virtual void RenderBox(const vec3f& bbMin, const vec3f& bbMax, const vec3f& color);
    virtual void ClearDebugBuffers();
#endif  // _DEBUG

protected:
    struct FrameConstants {
        mat4f    projView;
        mat4f    view;
        mat4f    projection;
        mat4f    invProjection;
        mat4f    invView;
        mat4f    invProjView;
        vec4f    cameraPos;
        vec4f    lightPosition;
        vec4f    lightPosInView;
        vec4f    lightIntensity;
        unsigned frameCount = 1;
        int      reset      = 0;
    };

    struct ObjectConstants {
        mat4f model;
        vec4f ambient;
        vec4f diffuse;
        vec4f emission;
        vec4f specular;
        float specularPower;
    };

    virtual void       InitConstants();
    virtual void       InitializeBuffers(const Resource::Scene& scene);
    virtual bool       InitializeShaders();
    virtual void       ClearShaders();
    virtual void       ClearBuffers();
    virtual void       CalculateCameraMatrix();
    virtual void       CalculateLights();
    virtual void       UpdateConstants();
    virtual void       RenderBuffers();
    const FT_GlyphSlot GetGlyph(char c);

    FrameConstants m_FrameConstants;
    ShaderManager  m_ShaderManager;

private:
    FT_Library   m_FTLibrary;
    FT_Face      m_FTFace;
    Core::Buffer m_FontFaceFile;
};

}  // namespace Hitagi::Graphics
namespace Hitagi {
extern std::unique_ptr<Graphics::GraphicsManager> g_GraphicsManager;
}