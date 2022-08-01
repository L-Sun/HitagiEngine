#pragma once
#include <hitagi/graphics/gpu_resource.hpp>
#include <hitagi/resource/mesh.hpp>

#include <functional>

namespace hitagi::graphics {
class IGraphicsCommandContext {
public:
    virtual ~IGraphicsCommandContext()                                                                              = default;
    virtual void SetViewPort(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)           = 0;
    virtual void SetScissorRect(std::uint32_t left, std::uint32_t top, std::uint32_t right, std::uint32_t bottom)   = 0;
    virtual void SetViewPortAndScissor(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height) = 0;
    virtual void SetRenderTarget(const RenderTarget& rt)                                                            = 0;
    virtual void UnsetRenderTarget()                                                                                = 0;
    virtual void SetRenderTargetAndDepthBuffer(const RenderTarget& rt, const DepthBuffer& depth_buffer)             = 0;
    virtual void ClearRenderTarget(const RenderTarget& rt)                                                          = 0;
    virtual void ClearDepthBuffer(const DepthBuffer& depth_buffer)                                                  = 0;
    virtual void SetPipelineState(const PipelineState& pipeline)                                                    = 0;
    virtual void BindResource(std::uint32_t slot, const ConstantBuffer& cb, size_t offset)                          = 0;
    virtual void BindResource(std::uint32_t slot, const resource::Texture& texture)                                 = 0;
    virtual void BindResource(std::uint32_t slot, const Sampler& sampler)                                           = 0;
    virtual void Set32BitsConstants(std::uint32_t slot, const std::uint32_t* data, std::size_t count)               = 0;

    virtual void Draw(const resource::VertexArray& vertices, const resource::IndexArray& indices, resource::PrimitiveType type,
                      std::size_t index_count, std::size_t vertex_offset, std::size_t index_offset) = 0;

    virtual void Present(const RenderTarget& rt) = 0;

    virtual void UpdateBuffer(backend::Resource* resource, size_t offset, const std::byte* data, size_t size) = 0;
    virtual void UpdateVertexBuffer(resource::VertexArray& vertices)                                          = 0;
    virtual void UpdateIndexBuffer(resource::IndexArray& indices)                                             = 0;

    virtual std::uint64_t Finish(bool wait_for_complete = false) = 0;
};

class IComputeCommadnContext {};

}  // namespace hitagi::graphics