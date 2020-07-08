#pragma once
#include "PipelineState.hpp"
#include "Resource.hpp"

namespace Hitagi::Graphics {
class IGraphicsCommandContext {
public:
    virtual ~IGraphicsCommandContext()                                                        = default;
    virtual void SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height)         = 0;
    virtual void SetRenderTarget(RenderTarget& rt)                                            = 0;
    virtual void SetRenderTargetAndDepthBuffer(RenderTarget& rt, DepthBuffer& depthBuffer)    = 0;
    virtual void SetPipelineState(const PipelineState& pso)                                   = 0;
    virtual void SetParameter(std::string_view name, const ConstantBuffer& cb, size_t offset) = 0;
    virtual void SetParameter(std::string_view name, const TextureBuffer& texture)            = 0;
    virtual void SetParameter(std::string_view name, const TextureSampler& sampler)           = 0;
    virtual void Draw(const MeshBuffer& mesh)                                                 = 0;
    virtual void Present(RenderTarget& rt)                                                    = 0;
    virtual void Finish(bool waitForComplete)                                                 = 0;
};

class IComputeCommadnContext {};

}  // namespace Hitagi::Graphics