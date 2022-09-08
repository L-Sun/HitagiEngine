#pragma once
#include <hitagi/graphics/gpu_resource.hpp>
#include <hitagi/resource/mesh.hpp>

#include <functional>

namespace hitagi::gfx {
class IGraphicsCommandContext {
public:
    virtual ~IGraphicsCommandContext() = default;

    virtual void ClearRenderTarget(const resource::Texture& rt)          = 0;
    virtual void ClearDepthBuffer(const resource::Texture& depth_buffer) = 0;

    virtual void SetRenderTarget(const resource::Texture& rt)                                                      = 0;
    virtual void SetRenderTargetAndDepthBuffer(const resource::Texture& rt, const resource::Texture& depth_buffer) = 0;
    virtual void SetPipelineState(const PipelineState& pipeline)                                                   = 0;

    virtual void SetViewPort(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)           = 0;
    virtual void SetScissorRect(std::uint32_t left, std::uint32_t top, std::uint32_t right, std::uint32_t bottom)   = 0;
    virtual void SetViewPortAndScissor(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height) = 0;
    virtual void SetBlendFactor(math::vec4f color)                                                                  = 0;

    // Resource Binding
    virtual void BindResource(std::uint32_t slot, const ConstantBuffer& cb, std::size_t index)            = 0;
    virtual void BindResource(std::uint32_t slot, const resource::Texture& texture)                       = 0;
    virtual void BindResource(std::uint32_t slot, const resource::Sampler& sampler)                       = 0;
    virtual void Set32BitsConstants(std::uint32_t slot, const std::uint32_t* data, std::size_t count)     = 0;
    virtual void BindDynamicStructuredBuffer(std::uint32_t slot, const std::byte* data, std::size_t size) = 0;
    virtual void BindDynamicConstantBuffer(std::uint32_t slot, const std::byte* data, std::size_t size)   = 0;

    // when dirty is set, it will be upload to gpu, (reallocated if buffer has been expanded)
    virtual void BindMeshBuffer(const resource::Mesh& mesh)              = 0;
    virtual void BindVertexBuffer(const resource::VertexArray& vertices) = 0;
    virtual void BindIndexBuffer(const resource::IndexArray& indices)    = 0;

    // Upload cpu buffer directly
    virtual void BindDynamicVertexBuffer(const resource::VertexArray& vertices) = 0;
    virtual void BindDynamicIndexBuffer(const resource::IndexArray& indices)    = 0;

    virtual void UpdateBuffer(gfx::backend::Resource* resource, std::size_t offset, const std::byte* data, std::size_t data_size) = 0;
    virtual void UpdateVertexBuffer(resource::VertexArray& vertices)                                                              = 0;
    virtual void UpdateIndexBuffer(resource::IndexArray& indices)                                                                 = 0;

    virtual void Draw(std::size_t vertex_count, std::size_t vertex_start_offset = 0)                                                                                                                            = 0;
    virtual void DrawIndexed(std::size_t index_count, std::size_t start_index_location = 0, std::size_t base_vertex_location = 0)                                                                               = 0;
    virtual void DrawInstanced(std::size_t vertex_count, std::size_t instance_count, std::size_t start_vertex_location = 0, std::size_t start_instance_location = 0)                                            = 0;
    virtual void DrawIndexedInstanced(std::size_t index_count, std::size_t instance_count, std::size_t start_index_location = 0, std::size_t base_vertex_location = 0, std::size_t start_instance_location = 0) = 0;

    virtual void Present(const resource::Texture& rt) = 0;

    virtual std::uint64_t Finish(bool wait_for_complete = false) = 0;
};

class IComputeCommadnContext {};

}  // namespace hitagi::gfx