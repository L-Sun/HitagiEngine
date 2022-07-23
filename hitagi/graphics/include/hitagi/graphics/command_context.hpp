#pragma once
#include <hitagi/graphics/resource.hpp>

#include <hitagi/resource/enums.hpp>
#include <functional>

namespace hitagi::graphics {
class IGraphicsCommandContext {
public:
    virtual ~IGraphicsCommandContext()                                                                                      = default;
    virtual void SetViewPort(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)                   = 0;
    virtual void SetScissorRect(std::uint32_t left, std::uint32_t top, std::uint32_t right, std::uint32_t bottom)           = 0;
    virtual void SetViewPortAndScissor(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)         = 0;
    virtual void SetRenderTarget(std::shared_ptr<RenderTarget> rt)                                                          = 0;
    virtual void UnsetRenderTarget()                                                                                        = 0;
    virtual void SetRenderTargetAndDepthBuffer(std::shared_ptr<RenderTarget> rt, std::shared_ptr<DepthBuffer> depth_buffer) = 0;
    virtual void ClearRenderTarget(std::shared_ptr<RenderTarget> rt)                                                        = 0;
    virtual void ClearDepthBuffer(std::shared_ptr<DepthBuffer> depth_buffer)                                                = 0;
    virtual void SetPipelineState(std::shared_ptr<PipelineState> pso)                                                       = 0;
    virtual void BindResource(std::uint32_t slot, std::shared_ptr<ConstantBuffer> cb, size_t offset)                        = 0;
    virtual void BindResource(std::uint32_t slot, std::shared_ptr<TextureBuffer> texture)                                   = 0;
    virtual void BindResource(std::uint32_t slot, std::shared_ptr<Sampler> sampler)                                         = 0;
    virtual void Set32BitsConstants(std::uint32_t slot, const std::uint32_t* data, std::size_t count)                       = 0;
    virtual void Draw(std::shared_ptr<VertexBuffer> vertices,
                      std::shared_ptr<IndexBuffer>  indices,
                      resource::PrimitiveType       type,
                      std::size_t                   index_count,
                      std::size_t                   vertex_offset,
                      std::size_t                   index_offset)                                                                             = 0;
    virtual void Present(std::shared_ptr<RenderTarget> rt)                                                                  = 0;

    virtual void UpdateBuffer(std::shared_ptr<Resource> resource, size_t offset, const std::byte* data, size_t size) = 0;

    virtual uint64_t Finish(bool wait_for_complete = false) = 0;
};

class IComputeCommadnContext {};

}  // namespace hitagi::graphics