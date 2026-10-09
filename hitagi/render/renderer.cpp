export module render:renderer;
import std;
import math;
import gfx;
import core;

import :types;

export namespace hitagi::render {

class IRenderer : public core::RuntimeModule {
public:
    using core::RuntimeModule::RuntimeModule;

    ~IRenderer() override = default;

    virtual auto Render(RenderContext& context, const RenderRequest& request) -> RenderResult = 0;
};

struct DeferredRenderResources {
    rg::TextureHandle   color;
    rg::TextureHandle   depth;
    rg::TextureHandle   linear_depth;
    rg::TextureHandle   object_id;
    rg::TextureHandle   material_id;
    rg::TextureHandle   gbuffer_albedo;
    rg::TextureHandle   gbuffer_normal;
    rg::TextureHandle   gbuffer_material;
    rg::TextureHandle   gbuffer_emissive;
    rg::TextureHandle   shadow_map;
    rg::GPUBufferHandle frame_constant;
    rg::SamplerHandle   sampler;
    std::uint32_t       width  = 0;
    std::uint32_t       height = 0;
};

struct DeferredDrawData {
    std::span<const InstanceInfo> instances;
    const RenderDrawState*        draw_state = nullptr;
};

class IDeferredRenderExtension {
public:
    virtual ~IDeferredRenderExtension() = default;

    virtual void AfterGBuffer(
        RenderContext&                 context,
        const RenderView&              view,
        const DeferredRenderResources& resources,
        const DeferredDrawData&        draw_data) {}

    virtual void AfterLighting(
        RenderContext&           context,
        const RenderView&        view,
        DeferredRenderResources& resources,
        const DeferredDrawData&  draw_data) {}
};

}  // namespace hitagi::render
