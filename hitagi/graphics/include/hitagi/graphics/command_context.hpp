#pragma once
#include "pipeline_state.hpp"
#include <hitagi/graphics/resource.hpp>

#include <hitagi/resource/enums.hpp>
#include <functional>

namespace hitagi::graphics {
class IGraphicsCommandContext {
public:
    virtual ~IGraphicsCommandContext()                                                                                            = default;
    virtual void SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height)                                             = 0;
    virtual void SetScissorRect(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)                                     = 0;
    virtual void SetViewPortAndScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height)                                   = 0;
    virtual void SetRenderTarget(std::shared_ptr<RenderTarget> rt)                                                                = 0;
    virtual void UnsetRenderTarget()                                                                                              = 0;
    virtual void SetRenderTargetAndDepthBuffer(std::shared_ptr<RenderTarget> rt, std::shared_ptr<DepthBuffer> depth_buffer)       = 0;
    virtual void ClearRenderTarget(std::shared_ptr<RenderTarget> rt)                                                              = 0;
    virtual void ClearDepthBuffer(std::shared_ptr<DepthBuffer> depth_buffer)                                                      = 0;
    virtual void SetPipelineState(std::shared_ptr<PipelineState> pso)                                                             = 0;
    virtual void SetParameter(std::string_view name, std::shared_ptr<ConstantBuffer> cb, size_t offset)                           = 0;
    virtual void SetParameter(std::string_view name, std::shared_ptr<TextureBuffer> texture)                                      = 0;
    virtual void SetParameter(std::string_view name, std::shared_ptr<Sampler> sampler)                                            = 0;
    virtual void Draw(std::shared_ptr<VertexBuffer> vertices, std::shared_ptr<IndexBuffer> indices, resource::PrimitiveType type) = 0;
    virtual void Present(std::shared_ptr<RenderTarget> rt)                                                                        = 0;

    virtual void UpdateBuffer(std::shared_ptr<Resource> resource, size_t offset, const std::byte* data, size_t size) = 0;

    virtual uint64_t Finish(bool wait_for_complete = false) = 0;
};

class IComputeCommadnContext {};

}  // namespace hitagi::graphics