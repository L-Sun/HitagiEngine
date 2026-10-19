module;
#include "interop/tracy_macros.hpp"

export module gfx.render_graph:pass_node;
import interop.tracy;
import std;
import utils;
import gfx.base;
import :type;
import :resource_edge;
import :resource_node;

export namespace hitagi::rg {

class PassNode : public RenderGraphNode {
public:
    friend RenderGraph;
    friend PassBuilder;

    ~PassNode() override = default;

    auto Resolve(GPUBufferHandle buffer) const -> gfx::GPUBuffer&;
    auto Resolve(GPUBufferEdgeHandle edge) const -> gfx::GPUBufferView&;
    auto Resolve(TextureEdgeHandle edge) const -> gfx::TextureView&;
    auto Resolve(TextureHandle texture) const -> gfx::Texture&;
    auto Resolve(SamplerHandle sampler) const -> gfx::Sampler&;

    auto GetCommandType() const noexcept -> gfx::CommandType;

protected:
    PassNode(RenderGraph& render_graph, Type type);

    void Initialize() final;

    void PrepareResourceBarriers();
    void ResourceBarrier();
    void PrepareResourceViews();

    virtual void Execute() = 0;

    std::pmr::vector<GPUBufferEdge>       m_GPUBufferEdges;
    std::pmr::vector<TextureEdge>         m_TextureEdges;
    std::uint64_t                         m_AccessOwner;
    std::pmr::unordered_set<SamplerNode*> m_Samplers;

    std::pmr::vector<gfx::GPUBufferBarrier> m_GPUBufferBarriers;
    std::pmr::vector<gfx::TextureBarrier>   m_TextureBarriers;
    bool                                    m_ResourceBarriersPrepared = false;

    std::shared_ptr<gfx::CommandContext> m_CommandContext;
    bool                                 m_Cullable = true;
};

class RenderPassNode : public PassNode {
public:
    friend RenderGraph;
    friend class RenderPassBuilder;

    using Executor = std::function<void(const RenderGraph&, const RenderPassNode&)>;

    RenderPassNode(RenderGraph& render_graph) : PassNode(render_graph, Type::RenderPass) {}

    inline auto& GetCmd() const noexcept { return static_cast<gfx::GraphicsCommandContext&>(*m_CommandContext); }

protected:
    void Execute() final;

    Executor          m_Executor;
    TextureEdgeHandle m_RenderTarget;
    TextureEdgeHandle m_DepthStencil;
    bool              m_ClearRenderTarget = false;
    bool              m_ClearDepthStencil = false;
};

class ComputePassNode : public PassNode {
public:
    friend RenderGraph;
    friend class ComputePassBuilder;

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
    friend class CopyPassBuilder;

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
    friend class PresentPassBuilder;

    using Executor = std::function<void(const RenderGraph&, const PresentPassNode&)>;

    PresentPassNode(RenderGraph& render_graph) : PassNode(render_graph, Type::PresentPass) {}

    inline auto& GetCmd() const noexcept { return static_cast<gfx::GraphicsCommandContext&>(*m_CommandContext); }

    std::shared_ptr<gfx::SwapChain> swap_chain;

protected:
    void Execute() final;

    Executor     m_Executor;
    TextureNode* m_From = nullptr;
};

}  // namespace hitagi::rg

namespace hitagi::rg {

auto ResourceNode::GetWriter() const noexcept -> PassNode* {
    for (auto* node : m_InputNodes) {
        if (node->IsPassNode()) return static_cast<PassNode*>(node);
    }
    return nullptr;
}

PassNode::PassNode(RenderGraph& graph, Type type) : RenderGraphNode(graph, type) {
    static std::uint64_t next_owner{1};
    m_AccessOwner = next_owner++;
}

auto PassNode::Resolve(GPUBufferEdgeHandle handle) const -> gfx::GPUBufferView& {
    if (handle.owner != m_AccessOwner || handle.index >= m_GPUBufferEdges.size()) {
        throw std::out_of_range("Access handle does not belong to this pass");
    }
    const auto& view = m_GPUBufferEdges[handle.index].view;
    if (!view) throw std::logic_error("Access has no view available");
    return *view;
}

auto PassNode::Resolve(TextureEdgeHandle handle) const -> gfx::TextureView& {
    if (handle.owner != m_AccessOwner || handle.index >= m_TextureEdges.size()) {
        throw std::out_of_range("Access handle does not belong to this pass");
    }
    const auto& view = m_TextureEdges[handle.index].view;
    if (!view) throw std::logic_error("Access has no view available");
    return *view;
}

auto PassNode::GetCommandType() const noexcept -> gfx::CommandType {
    switch (m_Type) {
        case Type::RenderPass:
        case Type::PresentPass:
            return gfx::CommandType::Graphics;
        case Type::ComputePass:
            return gfx::CommandType::Compute;
        case Type::CopyPass:
            return gfx::CommandType::Copy;
        default:
            utils::unreachable();
    }
}

void PassNode::ResourceBarrier() {
    if (!m_ResourceBarriersPrepared) {
        PrepareResourceBarriers();
    }

    m_CommandContext->ResourceBarrier({}, m_GPUBufferBarriers, m_TextureBarriers);
    m_ResourceBarriersPrepared = false;
}

void RenderPassNode::Execute() {
    ZoneScoped;
    ZoneName(m_Name.data(), m_Name.size());

    PrepareResourceViews();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();

    auto& render_target_view = Resolve(m_RenderTarget);
    cmd.BeginRendering(
        render_target_view,
        m_DepthStencil ? utils::make_optional_ref(Resolve(m_DepthStencil)) : std::nullopt,
        m_ClearRenderTarget,
        m_ClearDepthStencil);

    m_Executor(*m_RenderGraph, *this);

    cmd.EndRendering();
    cmd.End();
}

void ComputePassNode::Execute() {
    PrepareResourceViews();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();
    m_Executor(*m_RenderGraph, *this);
    cmd.End();
}

void CopyPassNode::Execute() {
    PrepareResourceViews();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();
    m_Executor(*m_RenderGraph, *this);
    cmd.End();
}

void PresentPassNode::Execute() {
    PrepareResourceViews();

    auto& cmd = GetCmd();
    cmd.Begin();
    ResourceBarrier();
    m_Executor(*m_RenderGraph, *this);
    cmd.End();
}

}  // namespace hitagi::rg
