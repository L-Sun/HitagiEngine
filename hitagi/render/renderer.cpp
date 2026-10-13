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
    rg::TextureHandle color;
    rg::TextureHandle depth;
    rg::TextureHandle linear_depth;
    rg::TextureHandle object_id;
    rg::TextureHandle material_id;
    rg::TextureHandle gbuffer_albedo;
    rg::TextureHandle gbuffer_normal;
    rg::TextureHandle gbuffer_material;
    rg::TextureHandle gbuffer_emissive;
    rg::TextureHandle shadow_map;
    // Must expose the engine ViewConstant prefix; lighting-specific data is opaque.
    rg::GPUBufferHandle frame_constant;
    rg::SamplerHandle   sampler;
    std::uint32_t       width  = 0;
    std::uint32_t       height = 0;
};

struct DeferredDrawData {
    std::span<const InstanceInfo> instances;
    const RenderGeometry*         draw_state = nullptr;
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

// Optional geometry-buffer extension capability; no concrete renderer dependency.
class IDeferredRenderer : public IRenderer {
public:
    using IRenderer::IRenderer;
    virtual void AddExtension(std::shared_ptr<IDeferredRenderExtension> extension) = 0;
    virtual void ClearExtensions()                                                 = 0;
};

// Engine's neutral default: initialize the requested target, without shading policy.
class ClearRenderer final : public IRenderer {
public:
    ClearRenderer() : IRenderer("ClearRenderer") {}
    auto Render(RenderContext& context, const RenderRequest& request) -> RenderResult final {
        if (!context.graph.IsValid(request.target)) return {.color = request.target};
        rg::RenderPassBuilder builder(context.graph);
        builder.SetName("Clear");
        builder.SetRenderTarget(request.target, true);
        builder.SetExecutor([](const rg::RenderGraph&, const rg::RenderPassNode&) {});
        builder.Finish();
        return {.color = request.target};
    }
};

}  // namespace hitagi::render
