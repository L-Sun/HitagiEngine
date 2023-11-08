#pragma once
#include <hitagi/render_graph/pass_node.hpp>

namespace hitagi::rg {

class PassBuilder {
public:
    friend RenderGraph;

    PassBuilder(const PassBuilder&)            = delete;
    PassBuilder(PassBuilder&&)                 = default;
    PassBuilder& operator=(const PassBuilder&) = delete;
    PassBuilder& operator=(PassBuilder&&)      = delete;

protected:
    PassBuilder(RenderGraph& render_graph, std::shared_ptr<PassNode> pass_base);

    ~PassBuilder();

    void Invalidate(std::string_view error_message) noexcept;
    void AddGPUBufferEdge(GPUBufferHandle buffer_handle, GPUBufferEdge edge) noexcept;
    void AddTextureEdge(TextureHandle texture_handle, TextureEdge edge) noexcept;
    void AddSamplerEdge(SamplerHandle sampler_handle, SamplerEdge edge) noexcept;

    RenderGraph& m_RenderGraph;
    bool         m_Invalid  = false;
    bool         m_Finished = false;

    std::shared_ptr<PassNode> pass_base;
};

class RenderPassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    RenderPassBuilder(RenderGraph& render_graph);

    RenderPassBuilder& SetName(std::string_view name) noexcept;

    RenderPassBuilder& Read(GPUBufferHandle buffer, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept;
    RenderPassBuilder& Read(GPUBufferHandle buffer, std::size_t element_offset, std::size_t num_elements, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept;
    RenderPassBuilder& Read(TextureHandle texture, gfx::TextureSubresourceLayer layer = {}, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept;
    RenderPassBuilder& ReadAsVertices(GPUBufferHandle buffer) noexcept;
    RenderPassBuilder& ReadAsIndices(GPUBufferHandle buffer) noexcept;

    RenderPassBuilder& Write(GPUBufferHandle buffer, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept;
    RenderPassBuilder& Write(GPUBufferHandle buffer, std::size_t element_offset, std::size_t num_elements, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept;
    RenderPassBuilder& Write(TextureHandle texture, gfx::TextureSubresourceLayer layer = {}, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept;

    RenderPassBuilder& SetRenderTarget(TextureHandle texture, bool clear = false, gfx::TextureSubresourceLayer layer = {}) noexcept;
    RenderPassBuilder& SetDepthStencil(TextureHandle texture, bool clear = false, gfx::TextureSubresourceLayer layer = {}) noexcept;

    RenderPassBuilder& AddSampler(SamplerHandle sampler) noexcept;
    RenderPassBuilder& AddPipeline(RenderPipelineHandle pipeline) noexcept;

    RenderPassBuilder& SetExecutor(RenderPassNode::Executor executor) noexcept;

    auto Finish() noexcept -> RenderPassHandle;

private:
    std::shared_ptr<RenderPassNode> pass;
};

class ComputePassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    ComputePassBuilder(RenderGraph& render_graph);

    ComputePassBuilder& SetName(std::string_view name) noexcept;

    ComputePassBuilder& Read(GPUBufferHandle buffer) noexcept;
    ComputePassBuilder& Read(GPUBufferHandle buffer, std::size_t element_offset, std::size_t num_elements) noexcept;
    ComputePassBuilder& Read(TextureHandle texture, gfx::TextureSubresourceLayer layer = {}) noexcept;

    ComputePassBuilder& Write(GPUBufferHandle buffer) noexcept;
    ComputePassBuilder& Write(GPUBufferHandle buffer, std::size_t element_offset, std::size_t num_elements) noexcept;
    ComputePassBuilder& Write(TextureHandle texture, gfx::TextureSubresourceLayer layer = {}) noexcept;

    ComputePassBuilder& AddSampler(SamplerHandle sampler) noexcept;
    ComputePassBuilder& AddPipeline(ComputePipelineHandle pipeline) noexcept;

    ComputePassBuilder& SetExecutor(ComputePassNode::Executor executor) noexcept;

    auto Finish() noexcept -> ComputePassHandle;

private:
    std::shared_ptr<ComputePassNode> pass;
};

class CopyPassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    CopyPassBuilder(RenderGraph& render_graph);

    CopyPassBuilder& SetName(std::string_view name) noexcept;

    CopyPassBuilder& BufferToBuffer(GPUBufferHandle src, GPUBufferHandle dst) noexcept;
    CopyPassBuilder& BufferToTexture(GPUBufferHandle src, TextureHandle dst, gfx::TextureSubresourceLayer layer = {}) noexcept;
    CopyPassBuilder& TextureToTexture(TextureHandle src, TextureHandle dst, gfx::TextureSubresourceLayer src_layer = {}, gfx::TextureSubresourceLayer dst_layer = {}) noexcept;

    CopyPassBuilder& SetExecutor(CopyPassNode::Executor executor) noexcept;

    auto Finish() noexcept -> CopyPassHandle;

private:
    std::shared_ptr<CopyPassNode> pass;
};

class PresentPassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    PresentPassBuilder(RenderGraph& render_graph);

    PresentPassBuilder& From(TextureHandle texture, gfx::TextureSubresourceLayer layer = {}) noexcept;
    PresentPassBuilder& SetSwapChain(const std::shared_ptr<gfx::SwapChain>& swap_chain) noexcept;

    void Finish() noexcept;

private:
    std::shared_ptr<PresentPassNode> pass;
};
}  // namespace hitagi::rg