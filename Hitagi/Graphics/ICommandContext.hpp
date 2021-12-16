#pragma once
#include "PipelineState.hpp"
#include "Resource.hpp"

namespace Hitagi::Graphics {
class IGraphicsCommandContext {
public:
    virtual ~IGraphicsCommandContext()                                                            = default;
    virtual void     SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height)         = 0;
    virtual void     SetRenderTarget(RenderTarget& rt)                                            = 0;
    virtual void     UnsetRenderTarget()                                                          = 0;
    virtual void     SetRenderTargetAndDepthBuffer(RenderTarget& rt, DepthBuffer& depth_buffer)   = 0;
    virtual void     ClearRenderTarget(RenderTarget& rt)                                          = 0;
    virtual void     ClearDepthBuffer(DepthBuffer& depth_buffer)                                  = 0;
    virtual void     SetPipelineState(const PipelineState& pso)                                   = 0;
    virtual void     SetParameter(std::string_view name, const ConstantBuffer& cb, size_t offset) = 0;
    virtual void     SetParameter(std::string_view name, const TextureBuffer& texture)            = 0;
    virtual void     SetParameter(std::string_view name, const Sampler& sampler)                  = 0;
    virtual void     Draw(const MeshBuffer& mesh)                                                 = 0;
    virtual void     Present(RenderTarget& rt)                                                    = 0;
    virtual uint64_t Finish(bool wait_for_complete = false)                                       = 0;
};

class IComputeCommadnContext {};

}  // namespace Hitagi::Graphics