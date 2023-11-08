#pragma once
#include <hitagi/render_graph/resource_edge.hpp>
#include <hitagi/gfx/command_context.hpp>

#include <unordered_set>

namespace hitagi::rg {

class PassBuilder;
class RenderPassBuilder;
class ComputePassBuilder;
class CopyPassBuilder;
class PresentPassBuilder;

class PassNode : public RenderGraphNode {
public:
    friend RenderGraph;
    friend PassBuilder;

    virtual ~PassNode() override;

    auto Resolve(GPUBufferHandle buffer) const -> gfx::GPUBuffer&;
    auto Resolve(TextureHandle texture) const -> gfx::Texture&;
    auto Resolve(SamplerHandle sampler) const -> gfx::Sampler&;
    auto Resolve(RenderPipelineHandle pipeline) const -> gfx::RenderPipeline&;
    auto Resolve(ComputePipelineHandle pipeline) const -> gfx::ComputePipeline&;

    auto GetBindless(GPUBufferHandle buffer, std::size_t index = 0) const noexcept -> gfx::BindlessHandle;
    auto GetBindless(TextureHandle buffer) const noexcept -> gfx::BindlessHandle;
    auto GetBindless(SamplerHandle sampler) const noexcept -> gfx::BindlessHandle;

    auto GetCommandType() const noexcept -> gfx::CommandType;

protected:
    PassNode(RenderGraph& render_graph, Type type) : RenderGraphNode(render_graph, type) {}

    void Initialize() final;

    void ResourceBarrier();
    void CreateBindless();

    virtual void Execute() = 0;

    std::pmr::unordered_map<GPUBufferHandle, GPUBufferEdge> m_GPUBufferEdges;
    std::pmr::unordered_map<TextureHandle, TextureEdge>     m_TextureEdges;
    std::pmr::unordered_map<SamplerHandle, SamplerEdge>     m_SamplerEdges;
    std::pmr::unordered_set<RenderPipelineHandle>           m_RenderPipelines;
    std::pmr::unordered_set<ComputePipelineHandle>          m_ComputePipelines;

    std::pmr::vector<gfx::GPUBufferBarrier> m_GPUBufferBarriers;
    std::pmr::vector<gfx::TextureBarrier>   m_TextureBarriers;

    std::shared_ptr<gfx::CommandContext> m_CommandContext;
};

class RenderPassNode : public PassNode {
public:
    friend RenderGraph;
    friend RenderPassBuilder;

    using Executor = std::function<void(const RenderGraph&, const RenderPassNode&)>;

    RenderPassNode(RenderGraph& render_graph) : PassNode(render_graph, Type::RenderPass) {}

    inline auto& GetCmd() const noexcept { return static_cast<gfx::GraphicsCommandContext&>(*m_CommandContext); }

protected:
    void Execute() final;

    Executor      m_Executor;
    TextureHandle m_RenderTarget;
    TextureHandle m_DepthStencil;
    bool          m_ClearRenderTarget = false;
    bool          m_ClearDepthStencil = false;
};

class ComputePassNode : public PassNode {
public:
    friend RenderGraph;
    friend ComputePassBuilder;

    using Executor = std::function<void(const RenderGraph&, const ComputePassNode&)>;

    ComputePassNode(RenderGraph& render_graph) : PassNode(render_graph, Type::ComputePass) {}

    inline auto& GetCmd() const noexcept { return static_cast<gfx::ComputeCommandContext&>(*m_CommandContext); }

protected:
    void Execute() final;

    Executor m_Executor;
};

class CopyPassNode : public PassNode {
public:
    friend RenderGraph;
    friend CopyPassBuilder;

    using Executor = std::function<void(const RenderGraph&, const CopyPassNode&)>;

    CopyPassNode(RenderGraph& render_graph) : PassNode(render_graph, Type::CopyPass) {}

    inline auto& GetCmd() const noexcept { return static_cast<gfx::CopyCommandContext&>(*m_CommandContext); }

protected:
    void Execute() final;

    Executor m_Executor;
};

class PresentPassNode : public PassNode {
public:
    friend RenderGraph;
    friend PresentPassBuilder;

    using Executor = std::function<void(const RenderGraph&, const PresentPassNode&)>;

    PresentPassNode(RenderGraph& render_graph) : PassNode(render_graph, Type::PresentPass) {}

    inline auto& GetCmd() const noexcept { return static_cast<gfx::GraphicsCommandContext&>(*m_CommandContext); }

    std::shared_ptr<gfx::SwapChain> swap_chain;

protected:
    void Execute() final;

    Executor      m_Executor;
    TextureHandle m_From;
};

}  // namespace hitagi::rg
