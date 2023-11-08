#pragma once
#include <hitagi/render_graph/pass_node.hpp>
#include <hitagi/render_graph/resource_node.hpp>
#include <hitagi/render_graph/pass_builder.hpp>
#include <hitagi/gfx/device.hpp>
#include <hitagi/utils/overloaded.hpp>

#include <deque>
#include <unordered_set>
#include "hitagi/render_graph/common_types.hpp"

namespace hitagi::rg {

class RenderGraph {
public:
    friend class DependencyGraph;

    RenderGraph(gfx::Device& device, std::string_view name = "RenderGraph");
    ~RenderGraph();

    auto Import(std::shared_ptr<gfx::GPUBuffer> buffer, std::string_view name = "") noexcept -> GPUBufferHandle;
    auto Import(std::shared_ptr<gfx::Texture> texture, std::string_view name = "") noexcept -> TextureHandle;
    auto Import(std::shared_ptr<gfx::Sampler> sampler, std::string_view name = "") noexcept -> SamplerHandle;
    auto Import(std::shared_ptr<gfx::RenderPipeline> pipeline, std::string_view name = "") noexcept -> RenderPipelineHandle;
    auto Import(std::shared_ptr<gfx::ComputePipeline> pipeline, std::string_view name = "") noexcept -> ComputePipelineHandle;

    auto Create(gfx::GPUBufferDesc desc, std::string_view name = "") noexcept -> GPUBufferHandle;
    auto Create(gfx::TextureDesc desc, std::string_view name = "") noexcept -> TextureHandle;
    auto Create(gfx::SamplerDesc desc, std::string_view name = "") noexcept -> SamplerHandle;
    auto Create(gfx::RenderPipelineDesc desc, std::string_view name = "") noexcept -> RenderPipelineHandle;
    auto Create(gfx::ComputePipelineDesc desc, std::string_view name = "") noexcept -> ComputePipelineHandle;

    template <RenderGraphNode::Type T>
    auto MoveFrom(RenderGraphHandle<T> resource, std::string_view name = "") noexcept -> RenderGraphHandle<T>;

    auto GetBufferHandle(std::string_view name) const noexcept -> GPUBufferHandle;
    auto GetTextureHandle(std::string_view name) const noexcept -> TextureHandle;
    auto GetSamplerHandle(std::string_view name) const noexcept -> SamplerHandle;
    auto GetRenderPipelineHandle(std::string_view name) const noexcept -> RenderPipelineHandle;
    auto GetComputePipelineHandle(std::string_view name) const noexcept -> ComputePipelineHandle;

    template <RenderGraphNode::Type T>
    auto& Resolve(RenderGraphHandle<T> handle) const;

    template <RenderGraphNode::Type T>
    auto& GetResourceDesc(RenderGraphHandle<T> handle) const;

    template <RenderGraphNode::Type T>
    bool IsValid(RenderGraphHandle<T> handle) const noexcept;

    bool Compile();
    auto Execute() -> std::uint64_t;

    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto  GetFrameIndex() const noexcept { return m_FrameIndex; }
    inline auto  GetLogger() const noexcept { return m_Logger; }

    auto ToDot() const noexcept -> std::pmr::string;

    void Profile() const noexcept;

private:
    friend PassBuilder;
    friend RenderPassBuilder;
    friend ComputePassBuilder;
    friend CopyPassBuilder;
    friend PresentPassBuilder;
    friend ResourceNode;

    using ResourceDesc  = std::variant<gfx::GPUBufferDesc, gfx::TextureDesc, gfx::SamplerDesc, gfx::RenderPipelineDesc, gfx::ComputePipelineDesc>;
    using AdjacencyList = std::pmr::unordered_map<std::size_t, std::pmr::unordered_set<std::size_t>>;
    using ExecuteLayer  = utils::EnumArray<std::pmr::vector<std::size_t>, gfx::CommandType>;
    struct FenceValue {
        std::shared_ptr<gfx::Fence> fence;
        std::uint64_t               last_value;
    };

    auto ImportResource(std::shared_ptr<gfx::Resource> resource, std::string_view name) noexcept -> std::size_t;
    auto CreateResource(ResourceDesc desc, std::string_view name) noexcept -> std::size_t;
    auto MoveFrom(RenderGraphNode::Type type, std::size_t resource_node_index, std::string_view name) noexcept -> std::size_t;

    inline bool IsValid(RenderGraphNode::Type type, std::size_t resource_node_index) const noexcept {
        return resource_node_index < m_Nodes.size() && m_Nodes[resource_node_index]->m_Type == type;
    }

    template <RenderGraphNode::Type T>
    auto GetHandle(std::string_view name) const noexcept;

    void RetireNodesFromPassNode(const std::shared_ptr<PassNode>& pass_node, const FenceValue& fence_value);
    void RetireNodes();
    void Reset();

    auto AdjacencyListToDot(const AdjacencyList& adjacency_list) const noexcept -> std::pmr::string;

    gfx::Device& m_Device;

    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;

    std::uint64_t m_FrameIndex = 0;

    std::pmr::vector<std::shared_ptr<RenderGraphNode>> m_Nodes;

    AdjacencyList                  m_DataFlow;
    std::pmr::vector<ExecuteLayer> m_ExecuteLayers;

    PresentPassHandle m_PresentPassHandle;

    using BlackBoard = utils::EnumArray<std::unordered_map<std::pmr::string, std::size_t>, RenderGraphNode::Type>;
    BlackBoard m_BlackBoard;

    utils::EnumArray<FenceValue, gfx::CommandType> m_Fences;

    struct RetiredNode {
        std::shared_ptr<RenderGraphNode> node;
        FenceValue                       last_fence_value;
    };
    std::pmr::deque<RetiredNode> m_RetiredNodes;
};

template <RenderGraphNode::Type T>
auto RenderGraph::MoveFrom(RenderGraphHandle<T> resource, std::string_view name) noexcept -> RenderGraphHandle<T> {
    return RenderGraphHandle<T>{MoveFrom(T, resource.index, name)};
}

template <RenderGraphNode::Type T>
auto RenderGraph::GetHandle(std::string_view name) const noexcept {
    const std::pmr::string _name(name);
    return RenderGraphHandle<T>{m_BlackBoard[T].contains(_name) ? m_BlackBoard[T].at(_name) : std::numeric_limits<std::size_t>::max()};
}

template <RenderGraphNode::Type T>
auto& RenderGraph::Resolve(RenderGraphHandle<T> handle) const {
    if (!IsValid(handle)) {
        throw std::out_of_range(fmt::format("Handle({}) is not valid", handle.index));
    }

    std::shared_ptr<RenderGraphNode> node = m_Nodes.at(handle.index);

    if constexpr (T == RenderGraphNode::Type::GPUBuffer) {
        return std::static_pointer_cast<GPUBufferNode>(node)->Resolve();
    } else if constexpr (T == RenderGraphNode::Type::Texture) {
        return std::static_pointer_cast<TextureNode>(node)->Resolve();
    } else if constexpr (T == RenderGraphNode::Type::Sampler) {
        return std::static_pointer_cast<SamplerNode>(node)->Resolve();
    } else if constexpr (T == RenderGraphNode::Type::RenderPipeline) {
        return std::static_pointer_cast<RenderPipelineNode>(node)->Resolve();
    } else if constexpr (T == RenderGraphNode::Type::ComputePipeline) {
        return std::static_pointer_cast<ComputePipelineNode>(node)->Resolve();
    } else if constexpr (T == RenderGraphNode::Type::RenderPass) {
        return std::static_pointer_cast<RenderPassNode>(node)->GetCmd();
    } else if constexpr (T == RenderGraphNode::Type::ComputePass) {
        return std::static_pointer_cast<ComputePassNode>(node)->GetCmd();
    } else if constexpr (T == RenderGraphNode::Type::CopyPass) {
        return std::static_pointer_cast<CopyPassNode>(node)->GetCmd();
    } else if constexpr (T == RenderGraphNode::Type::PresentPass) {
        return std::static_pointer_cast<PresentPassNode>(node)->GetCmd();
    } else {
        utils::unreachable();
    }
}

template <RenderGraphNode::Type T>
auto& RenderGraph::GetResourceDesc(RenderGraphHandle<T> handle) const {
    if (!IsValid(handle)) {
        throw std::out_of_range(fmt::format("Handle({}) is not valid", handle.index));
    }

    auto node = m_Nodes.at(handle.index);

    if constexpr (T == RenderGraphNode::Type::GPUBuffer) {
        return std::static_pointer_cast<GPUBufferNode>(node)->GetDesc();
    } else if constexpr (T == RenderGraphNode::Type::Texture) {
        return std::static_pointer_cast<TextureNode>(node)->GetDesc();
    } else if constexpr (T == RenderGraphNode::Type::Sampler) {
        return std::static_pointer_cast<SamplerNode>(node)->GetDesc();
    } else if constexpr (T == RenderGraphNode::Type::RenderPipeline) {
        return std::static_pointer_cast<RenderPipelineNode>(node)->GetDesc();
    } else if constexpr (T == RenderGraphNode::Type::ComputePipeline) {
        return std::static_pointer_cast<ComputePipelineNode>(node)->GetDesc();
    } else {
        utils::unreachable();
    }
}

template <RenderGraphNode::Type T>
bool RenderGraph::IsValid(RenderGraphHandle<T> handle) const noexcept {
    return IsValid(T, handle.index);
}

}  // namespace hitagi::rg