module;
#include "interop/tracy_macros.hpp"

export module gfx.render_graph:graph;
import interop.fmt;
import interop.spdlog;
import interop.tracy;
import interop.magic_enum;

import std;
import utils;
import gfx.base;
import :type;
import :resource_edge;
import :resource_node;
import :pass_node;
import :pass_builder;

export namespace hitagi::rg {

class RenderPassBuilder;
class ComputePassBuilder;
class CopyPassBuilder;
class PresentPassBuilder;

class RenderGraph {
public:
    friend class DependencyGraph;

    RenderGraph(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings, std::string_view name = "RenderGraph");
    ~RenderGraph();

    auto Import(std::shared_ptr<gfx::GPUBuffer> buffer, std::string_view name = "") noexcept -> GPUBufferHandle;
    auto Import(std::shared_ptr<gfx::Texture> texture, std::string_view name = "") noexcept -> TextureHandle;
    auto Import(std::shared_ptr<gfx::Sampler> sampler, std::string_view name = "") noexcept -> SamplerHandle;

    auto Create(gfx::GPUBufferDesc desc, std::string_view name = "") noexcept -> GPUBufferHandle;
    auto Create(gfx::TextureDesc desc, std::string_view name = "") noexcept -> TextureHandle;
    auto Create(gfx::SamplerDesc desc, std::string_view name = "") noexcept -> SamplerHandle;

    template <RenderGraphNode::Type T>
    auto MoveFrom(RenderGraphHandle<T> resource, std::string_view name = "") noexcept -> RenderGraphHandle<T>;

    auto GetBufferHandle(std::string_view name) const noexcept -> GPUBufferHandle;
    auto GetTextureHandle(std::string_view name) const noexcept -> TextureHandle;
    auto GetSamplerHandle(std::string_view name) const noexcept -> SamplerHandle;

    template <RenderGraphNode::Type T>
    auto& Resolve(RenderGraphHandle<T> handle) const;

    template <RenderGraphNode::Type T>
    auto& GetResourceDesc(RenderGraphHandle<T> handle) const;

    template <RenderGraphNode::Type T>
    bool IsValid(RenderGraphHandle<T> handle) const noexcept;

    auto QueueTextureExtraction(TextureHandle from, std::shared_ptr<gfx::Texture> to, gfx::TextureSubresourceLayer from_layer = {}, gfx::TextureSubresourceLayer to_layer = {}) noexcept -> CopyPassHandle;
    auto QueueBufferExtraction(TextureHandle from, std::shared_ptr<gfx::GPUBuffer> to, gfx::TextureSubresourceLayer from_layer = {}) noexcept -> CopyPassHandle;

    bool Compile();
    auto Execute() -> std::uint64_t;
    void ClearImportedResources() noexcept;

    inline auto& GetDevice() const noexcept { return m_Device; }
    inline auto& GetQueues() const noexcept { return m_Queues; }
    inline auto& GetBindings() const noexcept { return m_Bindings; }
    inline auto  GetFrameIndex() const noexcept { return m_FrameIndex; }
    inline auto  GetLogger() const noexcept { return m_Logger; }

    struct TransientPoolStats {
        std::uint64_t buffer_bytes  = 0;
        std::uint64_t texture_bytes = 0;
        std::size_t   buffer_count  = 0;
        std::size_t   texture_count = 0;
    };

    auto GetTransientPoolStats() const noexcept -> TransientPoolStats;

    auto ToDot() const noexcept -> std::pmr::string;

    void Profile() const noexcept;

private:
    friend PassBuilder;
    friend RenderPassBuilder;
    friend ComputePassBuilder;
    friend CopyPassBuilder;
    friend PresentPassBuilder;
    friend ResourceNode;
    friend GPUBufferNode;
    friend TextureNode;
    friend PassNode;

    using ResourceDesc = std::variant<gfx::GPUBufferDesc, gfx::TextureDesc, gfx::SamplerDesc>;
    using ExecuteLayer = utils::EnumArray<std::pmr::vector<PassNode*>, gfx::CommandType>;
    struct FenceValue {
        std::shared_ptr<gfx::Fence> fence;
        std::uint64_t               last_value;
    };

    auto ImportResource(std::shared_ptr<gfx::Resource> resource, std::string_view name) noexcept -> std::size_t;
    auto CreateResource(ResourceDesc desc, std::string_view name) noexcept -> std::size_t;
    auto MoveFrom(RenderGraphNode::Type type, std::size_t resource_node_index, std::string_view name) noexcept -> std::size_t;
    auto AllocateNode(std::shared_ptr<RenderGraphNode> node) noexcept -> std::size_t;
    void RebuildBlackBoard() noexcept;

    inline bool IsValid(RenderGraphNode::Type type, std::size_t resource_node_index) const noexcept {
        return resource_node_index < m_Nodes.size() && m_Nodes[resource_node_index] && m_Nodes[resource_node_index]->m_Type == type;
    }

    template <RenderGraphNode::Type T>
    auto GetHandle(std::string_view name) const noexcept;

    void RetireNodesFromPassNode(PassNode* pass_node, const FenceValue& fence_value) noexcept;
    void RetireNodes() noexcept;
    void Reset() noexcept;
    void ClearTransientResources() noexcept;

    auto AcquireTransientBuffer(const gfx::GPUBufferDesc& desc) -> std::shared_ptr<gfx::GPUBuffer>;
    auto AcquireTransientTexture(const gfx::TextureDesc& desc) -> std::shared_ptr<gfx::Texture>;
    void RecycleTransientResource(RenderGraphNode* node) noexcept;
    void EvictStalePoolEntries() noexcept;

    gfx::Device&        m_Device;
    gfx::CommandQueues& m_Queues;
    gfx::BindlessUtils& m_Bindings;

    std::pmr::string                m_Name;
    std::shared_ptr<spdlog::logger> m_Logger;

    std::uint64_t m_FrameIndex = 0;

    std::pmr::vector<std::shared_ptr<RenderGraphNode>>                   m_Nodes;
    std::pmr::unordered_map<std::shared_ptr<gfx::Resource>, std::size_t> m_ImportedResources;
    std::pmr::vector<std::size_t>                                        m_FreeNodeSlots;

    bool                           m_Compiled = false;
    std::pmr::vector<ExecuteLayer> m_ExecuteLayers;
    std::uint64_t                  m_ExtractionIndex = 0;

    std::shared_ptr<PresentPassNode> m_PresentPassNode;

    using BlackBoard = utils::EnumArray<std::unordered_map<std::pmr::string, std::size_t>, RenderGraphNode::Type>;
    BlackBoard m_BlackBoard;

    utils::EnumArray<FenceValue, gfx::CommandType> m_Fences;

    struct RetiredNode {
        std::shared_ptr<RenderGraphNode> node;
        FenceValue                       last_fence_value;
    };
    std::pmr::deque<RetiredNode> m_RetiredNodes;

    struct LayerProfile {
        double record_ms = 0.0;
        double submit_ms = 0.0;
    };
    std::pmr::vector<LayerProfile> m_LastLayerProfiles;

    struct TransientResourcePool {
        static constexpr std::uint64_t max_unused_frames      = 3;
        static constexpr std::uint64_t max_texture_pool_bytes = 128ull * 1024ull * 1024ull;

        struct CachedBuffer {
            gfx::GPUBufferDesc              desc;
            std::shared_ptr<gfx::GPUBuffer> resource;
            std::uint64_t                   last_used_frame = 0;
            std::uint64_t                   byte_size       = 0;
        };
        struct CachedTexture {
            gfx::TextureDesc              desc;
            std::shared_ptr<gfx::Texture> resource;
            std::uint64_t                 last_used_frame = 0;
            std::uint64_t                 byte_size       = 0;
        };

        std::unordered_multimap<std::size_t, CachedBuffer>  buffers;
        std::unordered_multimap<std::size_t, CachedTexture> textures;
        std::uint64_t                                       buffer_bytes  = 0;
        std::uint64_t                                       texture_bytes = 0;
    };
    TransientResourcePool m_TransientPool;
};

}  // namespace hitagi::rg

namespace hitagi::rg {

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
bool RenderGraph::IsValid(RenderGraphHandle<T> handle) const noexcept {
    return IsValid(T, handle.index);
}

}  // namespace hitagi::rg

namespace hitagi::rg {

template <RenderGraphNode::Type T>
auto& RenderGraph::GetResourceDesc(RenderGraphHandle<T> handle) const {
    if (!IsValid(handle)) {
        throw std::out_of_range(std::format("Handle({}) is not valid", handle.index));
    }

    auto node = m_Nodes.at(handle.index);

    if constexpr (T == RenderGraphNode::Type::GPUBuffer) {
        return std::static_pointer_cast<GPUBufferNode>(node)->GetDesc();
    } else if constexpr (T == RenderGraphNode::Type::Texture) {
        return std::static_pointer_cast<TextureNode>(node)->GetDesc();
    } else if constexpr (T == RenderGraphNode::Type::Sampler) {
        return std::static_pointer_cast<SamplerNode>(node)->GetDesc();
    } else {
        utils::unreachable();
    }
}

auto GPUBufferNode::Move(GPUBufferHandle new_handle, std::string_view new_name) -> std::shared_ptr<GPUBufferNode> {
    if (m_MoveToNode != nullptr) {
        auto error_message = std::format("GPUBufferNode::MoveTo: already moved to {}", m_MoveToNode->GetName());
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }

    std::shared_ptr<GPUBufferNode> new_node;
    if (m_IsImported) {
        new_node = std::make_shared<GPUBufferNode>(*m_RenderGraph, std::static_pointer_cast<gfx::GPUBuffer>(m_Resource), new_name);
    } else {
        new_node = std::make_shared<GPUBufferNode>(*m_RenderGraph, GetDesc(), new_name);
    }

    new_node->AddInputNode(this);
    m_MoveToNode             = new_node.get();
    new_node->m_MoveFromNode = this;
    new_node->m_Handle       = new_handle.index;

    return new_node;
}

void GPUBufferNode::Initialize() {
    if (m_MoveFromNode) {
        if (m_MoveFromNode->m_Resource == nullptr) m_MoveFromNode->Initialize();
        m_Resource = m_MoveFromNode->m_Resource;
    }
    if (m_IsImported || m_Resource) return;
    m_Resource = m_RenderGraph->AcquireTransientBuffer(m_Desc.value());
}

auto TextureNode::Move(TextureHandle new_handle, std::string_view new_name) -> std::shared_ptr<TextureNode> {
    if (m_MoveToNode != nullptr) {
        auto error_message = std::format("TextureNode::MoveTo: already moved to {}", m_MoveToNode->GetName());
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::invalid_argument(error_message);
    }

    std::shared_ptr<TextureNode> new_node;
    if (m_IsImported) {
        new_node = std::make_shared<TextureNode>(*m_RenderGraph, std::static_pointer_cast<gfx::Texture>(m_Resource), new_name);
    } else {
        new_node = std::make_shared<TextureNode>(*m_RenderGraph, GetDesc(), new_name);
    }

    new_node->AddInputNode(this);
    m_MoveToNode             = new_node.get();
    new_node->m_MoveFromNode = this;
    new_node->m_Handle       = new_handle.index;

    return new_node;
}

void TextureNode::Initialize() {
    if (m_MoveFromNode) {
        if (m_MoveFromNode->m_Resource == nullptr) m_MoveFromNode->Initialize();
        m_Resource = m_MoveFromNode->m_Resource;
    }
    if (m_IsImported || m_Resource) return;
    m_Resource = m_RenderGraph->AcquireTransientTexture(m_Desc.value());
}

void SamplerNode::Initialize() {
    if (m_IsImported || m_Resource) return;

    m_Resource = hitagi::gfx::Sampler::Create(m_RenderGraph->GetDevice(), m_RenderGraph->GetBindings(), m_Desc.value());
}

auto PassNode::Resolve(GPUBufferHandle buffer) const -> gfx::GPUBuffer& {
    if (!m_RenderGraph->IsValid(buffer)) {
        std::string error_message = std::format("Buffer({}) is not valid handle", buffer.index);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    const auto buffer_node = static_cast<GPUBufferNode*>(m_RenderGraph->m_Nodes[buffer.index].get());

    if (!std::ranges::any_of(m_GPUBufferEdges, [buffer_node](const auto& edge) { return edge.resource == buffer_node; })) {
        std::string error_message = std::format("Buffer({}) is not used in pass({})", buffer.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return buffer_node->Resolve();
}

auto PassNode::Resolve(TextureHandle texture) const -> gfx::Texture& {
    if (!m_RenderGraph->IsValid(texture)) {
        std::string error_message = std::format("Texture({}) is not valid handle", texture.index);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    const auto texture_node = static_cast<TextureNode*>(m_RenderGraph->m_Nodes[texture.index].get());

    if (!std::ranges::any_of(m_TextureEdges, [texture_node](const auto& edge) { return edge.resource == texture_node; })) {
        std::string error_message = std::format("Texture({}) is not used in pass({})", texture.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return texture_node->Resolve();
}

auto PassNode::Resolve(SamplerHandle sampler) const -> gfx::Sampler& {
    if (!m_RenderGraph->IsValid(sampler)) {
        std::string error_message = std::format("Sampler({}) is not valid handle", sampler.index);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    const auto sampler_node = static_cast<SamplerNode*>(m_RenderGraph->m_Nodes[sampler.index].get());

    if (!m_Samplers.contains(sampler_node)) {
        std::string error_message = std::format("Sampler({}) is not used in pass({})", sampler.index, m_Name);
        m_RenderGraph->GetLogger()->error(error_message);
        throw std::out_of_range(error_message);
    }

    return sampler_node->Resolve();
}

void PassNode::Initialize() {
    auto& device = m_RenderGraph->GetDevice();

    if (m_CommandContext == nullptr)
        m_CommandContext = hitagi::gfx::CommandContext::Create(device, m_RenderGraph->GetQueues(), m_RenderGraph->GetBindings(), GetCommandType(), GetName());

    // Views are prepared immediately before recording each pass.
}

void PassNode::PrepareResourceBarriers() {
    auto& device = m_RenderGraph->GetDevice();

    m_GPUBufferBarriers.clear();
    m_TextureBarriers.clear();

    // Aggregate by physical resource: multiple access views do not imply independent barriers.
    struct BufferAccess {
        gfx::GPUBuffer*    resource;
        bool               write;
        gfx::BarrierAccess access;
        gfx::PipelineStage stage;
    };
    struct TextureAccess {
        gfx::Texture*      resource;
        bool               write;
        gfx::BarrierAccess access;
        gfx::PipelineStage stage;
        gfx::TextureLayout layout;
    };
    std::vector<BufferAccess>  buffers;
    std::vector<TextureAccess> textures;
    for (const auto& edge : m_GPUBufferEdges) {
        auto* resource = &edge.resource->Resolve();
        auto  found    = std::ranges::find(buffers, resource, &BufferAccess::resource);
        if (found == buffers.end())
            buffers.push_back({.resource = resource, .write = edge.write, .access = edge.access, .stage = edge.stage});
        else {
            if (found->write != edge.write) throw std::invalid_argument("Conflicting accesses to an aliased buffer");
            found->access |= edge.access;
            found->stage |= edge.stage;
        }
    }
    for (const auto& edge : m_TextureEdges) {
        auto* resource = &edge.resource->Resolve();
        auto  found    = std::ranges::find(textures, resource, &TextureAccess::resource);
        if (found == textures.end())
            textures.push_back({.resource = resource, .write = edge.write, .access = edge.access, .stage = edge.stage, .layout = edge.layout});
        else {
            if (found->write != edge.write || found->layout != edge.layout) throw std::invalid_argument("Conflicting accesses to an aliased texture");
            found->access |= edge.access;
            found->stage |= edge.stage;
        }
    }
    for (const auto& access : buffers)
        m_GPUBufferBarriers.emplace_back(access.resource->Transition(access.access, access.stage));
    for (const auto& access : textures)
        m_TextureBarriers.emplace_back(access.resource->Transition(access.access, access.layout, access.stage));

    // https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html#command-queue-layout-compatibility
    if (device.device_type == gfx::Device::Type::DX12) {
        if (GetCommandType() == gfx::CommandType::Copy) {
            for (auto& buffer_barrier : m_GPUBufferBarriers) {
                if (buffer_barrier.src_access != gfx::BarrierAccess::CopySrc &&
                    buffer_barrier.src_access != gfx::BarrierAccess::CopyDst) {
                    buffer_barrier.src_access = gfx::BarrierAccess::None;
                }
                if (buffer_barrier.src_stage != gfx::PipelineStage::Copy) {
                    buffer_barrier.src_stage = gfx::PipelineStage::None;
                }
            }
            for (auto& texture_barrier : m_TextureBarriers) {
                if (texture_barrier.src_access != gfx::BarrierAccess::CopySrc &&
                    texture_barrier.src_access != gfx::BarrierAccess::CopyDst) {
                    texture_barrier.src_access = gfx::BarrierAccess::None;
                }
                if (texture_barrier.src_stage != gfx::PipelineStage::Copy) {
                    texture_barrier.src_stage = gfx::PipelineStage::None;
                }
                if (texture_barrier.src_layout != gfx::TextureLayout::Common) {
                    texture_barrier.src_layout = gfx::TextureLayout::Unkown;
                }
                texture_barrier.dst_layout = gfx::TextureLayout::Common;
            }
        }
    }

    m_ResourceBarriersPrepared = true;
}

void PassNode::PrepareResourceViews() {
    auto& device = m_RenderGraph->GetDevice();
    for (auto& edge : m_GPUBufferEdges) {
        if (!edge.create_view || edge.view) continue;
        auto desc   = edge.view_desc;
        desc.buffer = edge.resource->GetBuffer();
        edge.view   = hitagi::gfx::GPUBufferView::Create(device, m_RenderGraph->GetBindings(), std::move(desc));
    }
    for (auto& edge : m_TextureEdges) {
        if (!edge.create_view || edge.view) continue;
        auto desc    = edge.view_desc;
        desc.texture = edge.resource->GetTexture();
        edge.view    = hitagi::gfx::TextureView::Create(device, m_RenderGraph->GetBindings(), std::move(desc));
    }
}

PassBuilder::~PassBuilder() {
    if (!m_Finished) {
        m_RenderGraph.GetLogger()->warn("PassBuilder is not finished, remove the node from graph");
    }
}

auto PassBuilder::Finish() -> std::size_t {
    pass_base->m_Handle = m_RenderGraph.AllocateNode(pass_base);

    // create edge for move resource:
    // resource_1 -- move --> resource_2(*)        resource_1 -- new_edge -->  pass_2
    //      |                    |read               |                         | write
    //      |                    v         OR        |                         v
    //      +--- new_edge -->  pass_2                +--------- move --- >  resource_2(*)

    for (auto& buffer_edge : pass_base->m_GPUBufferEdges) {
        auto* buffer_node = buffer_edge.resource;
        if (buffer_edge.write) {
            buffer_node->AddInputNode(pass_base.get());
        } else {
            pass_base->AddInputNode(buffer_node);
        }

        if (const auto buffer_move_from_node = buffer_node->GetMoveFromNode();
            buffer_move_from_node != nullptr) {
            pass_base->AddInputNode(buffer_move_from_node);
        }
    }

    for (const auto& texture_edge : pass_base->m_TextureEdges) {
        auto* texture_node = texture_edge.resource;
        if (texture_edge.write) {
            texture_node->AddInputNode(pass_base.get());
        } else {
            pass_base->AddInputNode(texture_node);
        }

        if (const auto texture_move_from_node = texture_node->GetMoveFromNode();
            texture_move_from_node != nullptr) {
            pass_base->AddInputNode(texture_move_from_node);
        }
    }

    for (auto* sampler_node : pass_base->m_Samplers) {
        pass_base->AddInputNode(sampler_node);
    }

    m_Finished = true;

    return pass_base->m_Handle;
}

void PassBuilder::Invalidate(std::string_view error_message) noexcept {
    if (!m_Invalid) {
        m_Invalid = true;
        m_RenderGraph.m_Logger->error(error_message);
    }
}

auto PassBuilder::AddGPUBufferEdge(GPUBufferHandle handle, GPUBufferEdge edge) noexcept -> GPUBufferEdgeHandle {
    if (m_Invalid || m_Finished) return {};
    if (!m_RenderGraph.IsValid(handle)) {
        Invalidate("Invalid buffer handle");
        return {};
    }
    if (edge.view_desc.buffer) {
        Invalidate("Access descriptors must not contain a physical resource");
        return {};
    }
    auto*      node     = static_cast<GPUBufferNode*>(m_RenderGraph.m_Nodes[handle.index].get());
    const auto usages   = node->GetDesc().usages;
    const auto required = edge.access == gfx::BarrierAccess::Vertex    ? gfx::GPUBufferUsageFlags::Vertex
                          : edge.access == gfx::BarrierAccess::Index   ? gfx::GPUBufferUsageFlags::Index
                          : edge.access == gfx::BarrierAccess::CopySrc ? gfx::GPUBufferUsageFlags::CopySrc
                          : edge.access == gfx::BarrierAccess::CopyDst ? gfx::GPUBufferUsageFlags::CopyDst
                          : edge.write                                 ? gfx::GPUBufferUsageFlags::StorageWrite
                                                                       : gfx::GPUBufferUsageFlags::StorageRead;
    if (!utils::has_flag(usages, required)) {
        Invalidate("buffer usage does not support the declared access");
        return {};
    }
    for (const auto& existing : pass_base->m_GPUBufferEdges) {
        if (existing.resource != node) continue;
        if (existing.write != edge.write) {
            Invalidate("Incompatible accesses to the same buffer in one pass");
            return {};
        }
    }
    if (edge.write && node->GetWriter() != nullptr) {
        Invalidate("buffer already has a writer");
        return {};
    }
    edge.resource = node;
    const GPUBufferEdgeHandle result{.owner = pass_base->m_AccessOwner, .index = pass_base->m_GPUBufferEdges.size()};
    pass_base->m_GPUBufferEdges.push_back(std::move(edge));
    return result;
}

auto PassBuilder::AddTextureEdge(TextureHandle handle, TextureEdge edge) noexcept -> TextureEdgeHandle {
    if (m_Invalid || m_Finished) return {};
    if (!m_RenderGraph.IsValid(handle)) {
        Invalidate("Invalid texture handle");
        return {};
    }
    if (edge.view_desc.texture) {
        Invalidate("Access descriptors must not contain a physical resource");
        return {};
    }
    auto*      node     = static_cast<TextureNode*>(m_RenderGraph.m_Nodes[handle.index].get());
    const auto usages   = node->GetDesc().usages;
    const auto required = edge.access == gfx::BarrierAccess::RenderTarget                                                                 ? gfx::TextureUsageFlags::RenderTarget
                          : (edge.access == gfx::BarrierAccess::DepthStencilRead || edge.access == gfx::BarrierAccess::DepthStencilWrite) ? gfx::TextureUsageFlags::DepthStencil
                          : edge.access == gfx::BarrierAccess::CopySrc                                                                    ? gfx::TextureUsageFlags::CopySrc
                          : edge.access == gfx::BarrierAccess::CopyDst                                                                    ? gfx::TextureUsageFlags::CopyDst
                          : edge.write                                                                                                    ? gfx::TextureUsageFlags::UAV
                                                                                                                                          : gfx::TextureUsageFlags::SRV;
    if (!utils::has_flag(usages, required)) {
        Invalidate("texture usage does not support the declared access");
        return {};
    }
    for (const auto& existing : pass_base->m_TextureEdges) {
        if (existing.resource != node) continue;
        if (existing.write != edge.write || existing.layout != edge.layout) {
            Invalidate("Incompatible accesses to the same texture in one pass");
            return {};
        }
    }
    if (edge.write && node->GetWriter() != nullptr) {
        Invalidate("texture already has a writer");
        return {};
    }
    edge.resource = node;
    const TextureEdgeHandle result{.owner = pass_base->m_AccessOwner, .index = pass_base->m_TextureEdges.size()};
    pass_base->m_TextureEdges.push_back(std::move(edge));
    return result;
}

void PassBuilder::AddSamplerEdge(SamplerHandle sampler_handle) noexcept {
    ZoneScoped;

    if (m_Invalid) return;

    if (!m_RenderGraph.IsValid(sampler_handle)) {
        Invalidate(std::format("Read sampler failed: sampler({}) is invalid", sampler_handle.index));
        return;
    }

    const auto sampler_node = static_cast<SamplerNode*>(m_RenderGraph.m_Nodes[sampler_handle.index].get());

    if (pass_base->m_Samplers.contains(sampler_node)) {
        return;
    }

    pass_base->m_Samplers.emplace(sampler_node);
}

void RenderPassBuilder::SetName(std::string_view name) noexcept {
    if (m_Invalid) return;

    if (m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::RenderPass].contains(std::pmr::string(name))) {
        Invalidate(fmt::format("Set name failed: name ({}) already exists", fmt::styled(name, fmt::fg(fmt::color::red))));
    }

    pass->m_Name = name;
}

auto RenderPassBuilder::Finish() noexcept -> RenderPassHandle {
    if (m_Invalid) return {};
    if (m_Finished) {
        Invalidate("Finish render pass failed: pass is already finished");
    }
    if (m_Invalid) return {};
    {
        if (!pass->m_Executor) {
            Invalidate("Finish render pass failed: executor is not set");
        }
        if (!pass->m_RenderTarget) {
            Invalidate("Finish render pass failed: render target is not set");
        }
    }
    if (m_Invalid) return {};

    RenderPassHandle handle = PassBuilder::Finish();
    if (!pass->m_Name.empty()) {
        m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::RenderPass].emplace(pass->m_Name, handle.index);
    }
    return handle;
}

void ComputePassBuilder::SetName(std::string_view name) noexcept {
    if (m_Invalid) return;

    if (m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::ComputePass].contains(std::pmr::string(name))) {
        Invalidate(fmt::format("Set name failed: the name ({}) already exists", fmt::styled(name, fmt::fg(fmt::color::red))));
        return;
    }

    pass->m_Name = name;
}

auto ComputePassBuilder::Finish() noexcept -> ComputePassHandle {
    if (m_Invalid) return {};
    if (m_Finished) {
        Invalidate("Finish compute pass failed: pass is already finished");
    }
    if (m_Invalid) return {};
    {
        if (!pass->m_Executor) {
            Invalidate("Finish compute pass failed: executor is not set");
        }
    }
    if (m_Invalid) return {};

    ComputePassHandle handle = PassBuilder::Finish();
    if (!pass->m_Name.empty()) {
        m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::ComputePass].emplace(pass->m_Name, handle.index);
    }
    return handle;
}

void CopyPassBuilder::SetName(std::string_view name) noexcept {
    if (m_Invalid) return;

    if (m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::CopyPass].contains(std::pmr::string(name))) {
        Invalidate(fmt::format("Set name failed: name {} already exists", fmt::styled(name, fmt::fg(fmt::color::red))));
    }

    pass->m_Name = name;
}

auto CopyPassBuilder::Finish() noexcept -> CopyPassHandle {
    if (m_Invalid) return {};
    if (m_Finished) {
        Invalidate("Finish copy pass failed: pass is already finished");
    }
    if (m_Invalid) return {};
    {
        if (!pass->m_Executor) Invalidate("Finish copy pass failed: executor is not set");
    }
    if (m_Invalid) return {};

    CopyPassHandle handle = PassBuilder::Finish();

    if (!pass->m_Name.empty()) {
        m_RenderGraph.m_BlackBoard[RenderGraphNode::Type::CopyPass].emplace(pass->m_Name, handle.index);
    }

    return handle;
}

void PresentPassBuilder::From(TextureHandle texture, gfx::TextureSubresourceLayer layer) noexcept {
    if (pass->m_From) {
        Invalidate(std::format("Present from texture({}) failed: already presented from texture({})", texture.index, pass->m_From->GetName()));
        return;
    }

    AddTextureEdge(
        texture,
        {
            .write       = false,
            .access      = gfx::BarrierAccess::CopySrc,
            .stage       = gfx::PipelineStage::All,
            .layout      = gfx::TextureLayout::CopySrc,
            .view_desc   = {.base_mip_level = layer.mip_level, .mip_levels = 1, .base_array_layer = layer.base_array_layer, .layer_count = layer.layer_count},
            .create_view = false,
        });
    pass->m_From = static_cast<TextureNode*>(m_RenderGraph.m_Nodes[texture.index].get());
}

void PresentPassBuilder::Finish() noexcept {
    if (m_Invalid) return;
    if (m_Finished) {
        Invalidate("Finish present pass failed: pass is already finished");
    }
    if (m_Invalid) return;
    {
        if (!pass->swap_chain) {
            Invalidate(std::format("Set swap chain failed: swap chain is nullptr"));
        }

        if (!pass->m_From) {
            Invalidate(std::format("Set present source failed: present source is nullptr"));
        }
    }
    if (m_Invalid) return;

    if (m_RenderGraph.m_PresentPassNode != nullptr) {
        Invalidate("Finish present pass failed: a present pass already exists in the render graph");
        return;
    }

    PassBuilder::Finish();
    m_RenderGraph.m_PresentPassNode = pass;
}

RenderGraph::RenderGraph(gfx::Device& device, gfx::CommandQueues& queues, gfx::BindlessUtils& bindings, std::string_view name)
    : m_Device(device), m_Queues(queues), m_Bindings(bindings), m_Name(name), m_Logger(utils::try_create_logger(std::format("RenderGraph{}", utils::add_parentheses(name)))) {
    m_Fences[gfx::CommandType::Graphics] = {
        .fence      = hitagi::gfx::Fence::Create(m_Device, 0, "[RG] Render Fence"),
        .last_value = 0,
    };
    m_Fences[gfx::CommandType::Compute] = {
        .fence      = hitagi::gfx::Fence::Create(m_Device, 0, "[RG] Compute Fence"),
        .last_value = 0,
    };
    m_Fences[gfx::CommandType::Copy] = {
        .fence      = hitagi::gfx::Fence::Create(m_Device, 0, "[RG] Copy Fence"),
        .last_value = 0,
    };
}

RenderGraph::~RenderGraph() {
    m_Queues.WaitIdle();
}

auto RenderGraph::ImportResource(std::shared_ptr<gfx::Resource> resource, std::string_view name) noexcept -> std::size_t {
    constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();

    if (resource == nullptr) {
        m_Logger->error("Import resource failed: resource is nullptr");
        return invalid_index;
    }

    const std::pmr::string _name(name);
    const auto             node_type = gfx_resource_type_to_node_type(resource->GetType());

    if (m_BlackBoard[node_type].contains(_name)) {
        const auto handle        = m_BlackBoard[node_type].at(_name);
        const auto resource_node = std::static_pointer_cast<ResourceNode>(m_Nodes[handle]);

        if (resource_node->m_Resource != resource) {
            m_Logger->error("Import resource({}) failed: name {} already exists",
                            fmt::styled(resource->GetName(), fmt::fg(fmt::color::red)),
                            fmt::styled(name, fmt::fg(fmt::color::red)));
            return invalid_index;
        } else {
            return handle;
        }
    }

    if (m_ImportedResources.contains(resource)) {
        if (!name.empty()) {
            m_BlackBoard[node_type].emplace(name, m_ImportedResources.at(resource));
        }
        return m_ImportedResources.at(resource);
    }

    std::shared_ptr<ResourceNode> new_node;
    {
        ZoneScopedN("create new resource node");
        switch (node_type) {
            case hitagi::rg::RenderGraphNode::Type::GPUBuffer:
                new_node = std::make_shared<GPUBufferNode>(*this, resource, name);
                break;
            case hitagi::rg::RenderGraphNode::Type::Texture:
                new_node = std::make_shared<TextureNode>(*this, resource, name);
                break;
            case hitagi::rg::RenderGraphNode::Type::Sampler:
                new_node = std::make_shared<SamplerNode>(*this, resource, name);
                break;
            default:
                utils::unreachable();
        }
    }
    const auto handle = AllocateNode(new_node);
    m_ImportedResources.emplace(resource, handle);

    if (!name.empty()) {
        m_BlackBoard[node_type].emplace(name, handle);
    }

    return handle;
}

auto RenderGraph::Import(std::shared_ptr<gfx::GPUBuffer> buffer, std::string_view name) noexcept -> GPUBufferHandle {
    return ImportResource(std::move(buffer), name);
}

auto RenderGraph::Import(std::shared_ptr<gfx::Texture> texture, std::string_view name) noexcept -> TextureHandle {
    return ImportResource(std::move(texture), name);
}

auto RenderGraph::Import(std::shared_ptr<gfx::Sampler> sampler, std::string_view name) noexcept -> SamplerHandle {
    return ImportResource(std::move(sampler), name);
}

auto RenderGraph::CreateResource(ResourceDesc desc, std::string_view name) noexcept -> std::size_t {
    constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();

    RenderGraphNode::Type node_type;

    if (std::holds_alternative<gfx::GPUBufferDesc>(desc)) {
        node_type = RenderGraphNode::Type::GPUBuffer;
    } else if (std::holds_alternative<gfx::TextureDesc>(desc)) {
        node_type = RenderGraphNode::Type::Texture;
    } else if (std::holds_alternative<gfx::SamplerDesc>(desc)) {
        node_type = RenderGraphNode::Type::Sampler;
    }

    const std::pmr::string _name(name);

    if (m_BlackBoard[node_type].contains(_name)) {
        m_Logger->error("Create buffer failed: name {} already exists",
                        fmt::styled(name, fmt::fg(fmt::color::red)));
        return invalid_index;
    }

    std::shared_ptr<ResourceNode> new_node;
    switch (node_type) {
        case hitagi::rg::RenderGraphNode::Type::GPUBuffer:
            new_node = std::make_shared<GPUBufferNode>(*this, std::move(std::get<gfx::GPUBufferDesc>(desc)), name);
            break;
        case hitagi::rg::RenderGraphNode::Type::Texture:
            new_node = std::make_shared<TextureNode>(*this, std::move(std::get<gfx::TextureDesc>(desc)), name);
            break;
        case hitagi::rg::RenderGraphNode::Type::Sampler:
            new_node = std::make_shared<SamplerNode>(*this, std::move(std::get<gfx::SamplerDesc>(desc)), name);
            break;
        default:
            utils::unreachable();
    }

    const auto handle = AllocateNode(new_node);

    if (!name.empty()) {
        m_BlackBoard[node_type].emplace(name, handle);
    }

    return handle;
}

auto RenderGraph::Create(gfx::GPUBufferDesc desc, std::string_view name) noexcept -> GPUBufferHandle {
    return CreateResource(std::move(desc), name);
}

auto RenderGraph::Create(gfx::TextureDesc desc, std::string_view name) noexcept -> TextureHandle {
    return CreateResource(std::move(desc), name);
}

auto RenderGraph::Create(gfx::SamplerDesc desc, std::string_view name) noexcept -> SamplerHandle {
    return CreateResource(std::move(desc), name);
}

auto RenderGraph::MoveFrom(RenderGraphNode::Type type, std::size_t resource_node_index, std::string_view name) noexcept -> std::size_t {
    constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();

    if (!IsValid(type, resource_node_index)) {
        m_Logger->error("Move resource failed: resource is invalid");
        return invalid_index;
    }

    const std::pmr::string _name(name);
    if (m_BlackBoard[type].contains(_name)) {
        m_Logger->error("Move resource failed: name {} already exists",
                        fmt::styled(name, fmt::fg(fmt::color::red)));
        return invalid_index;
    }

    const auto new_handle = m_FreeNodeSlots.empty() ? m_Nodes.size() : m_FreeNodeSlots.back();
    switch (type) {
        case RenderGraphNode::Type::GPUBuffer: {
            const auto buffer_node = std::static_pointer_cast<GPUBufferNode>(m_Nodes[resource_node_index]);
            AllocateNode(buffer_node->Move(new_handle, name));
        } break;
        case RenderGraphNode::Type::Texture: {
            const auto texture_node = std::static_pointer_cast<TextureNode>(m_Nodes[resource_node_index]);
            AllocateNode(texture_node->Move(new_handle, name));
        } break;
        default: {
            m_Logger->error("Move resource failed: resource is not a GPUBuffer or Texture");
            return invalid_index;
        } break;
    }

    if (!name.empty()) {
        m_BlackBoard[type].emplace(name, new_handle);
    }

    return new_handle;
}

auto RenderGraph::GetBufferHandle(std::string_view name) const noexcept -> GPUBufferHandle {
    return GetHandle<RenderGraphNode::Type::GPUBuffer>(name);
}

auto RenderGraph::GetTextureHandle(std::string_view name) const noexcept -> TextureHandle {
    return GetHandle<RenderGraphNode::Type::Texture>(name);
}

auto RenderGraph::GetSamplerHandle(std::string_view name) const noexcept -> SamplerHandle {
    return GetHandle<RenderGraphNode::Type::Sampler>(name);
}

auto RenderGraph::QueueTextureExtraction(TextureHandle from, std::shared_ptr<gfx::Texture> to, gfx::TextureSubresourceLayer from_layer, gfx::TextureSubresourceLayer to_layer) noexcept -> CopyPassHandle {
    if (!IsValid(from)) {
        m_Logger->error("Queue texture extraction failed: source texture({}) is invalid", from.index);
        return {};
    }
    if (!to) {
        m_Logger->error("Queue texture extraction failed: destination texture is nullptr");
        return {};
    }

    const auto dst  = Import(std::move(to));
    const auto name = std::format("TextureExtraction-{}-{}", m_FrameIndex, m_ExtractionIndex++);

    CopyPassBuilder builder(*this);
    builder.SetName(name);
    builder.AllowPassCulling(false);
    builder.TextureToTexture(from, dst, from_layer, to_layer);
    builder.SetExecutor([from, dst, from_layer, to_layer](const RenderGraph&, const CopyPassNode& pass) {
        auto& cmd     = pass.GetCmd();
        auto& src     = pass.Resolve(from);
        auto& dst_tex = pass.Resolve(dst);

        cmd.CopyTextureRegion(
            src,
            {0, 0, 0},
            dst_tex,
            {0, 0, 0},
            {
                std::min(src.GetDesc().width, dst_tex.GetDesc().width),
                std::min(src.GetDesc().height, dst_tex.GetDesc().height),
                std::min(static_cast<std::uint32_t>(src.GetDesc().depth), static_cast<std::uint32_t>(dst_tex.GetDesc().depth)),
            },
            from_layer,
            to_layer);
    });
    return builder.Finish();
}

auto RenderGraph::QueueBufferExtraction(TextureHandle from, std::shared_ptr<gfx::GPUBuffer> to, gfx::TextureSubresourceLayer from_layer) noexcept -> CopyPassHandle {
    if (!IsValid(from)) {
        m_Logger->error("Queue buffer extraction failed: source texture({}) is invalid", from.index);
        return {};
    }
    if (!to) {
        m_Logger->error("Queue buffer extraction failed: destination buffer is nullptr");
        return {};
    }

    const auto dst  = Import(std::move(to));
    const auto name = std::format("BufferExtraction-{}-{}", m_FrameIndex, m_ExtractionIndex++);

    CopyPassBuilder builder(*this);
    builder.SetName(name);
    builder.AllowPassCulling(false);
    builder.TextureToBuffer(from, dst, from_layer);
    builder.SetExecutor([from, dst, from_layer](const RenderGraph&, const CopyPassNode& pass) {
        auto& cmd        = pass.GetCmd();
        auto& src        = pass.Resolve(from);
        auto& dst_buffer = pass.Resolve(dst);

        cmd.CopyTextureToBuffer(
            src,
            {0, 0, 0},
            {
                src.GetDesc().width,
                src.GetDesc().height,
                static_cast<std::uint32_t>(src.GetDesc().depth),
            },
            dst_buffer,
            0,
            from_layer);
    });
    return builder.Finish();
}

bool RenderGraph::Compile() {
    ZoneScoped;

    if (m_Compiled) {
        m_Logger->info("RenderGraph has already been compiled");
        return true;
    }

    const auto side_effect_roots = m_Nodes                                                                                               //
                                   | std::ranges::views::filter([](const auto& node) { return node && node->IsPassNode(); })             //
                                   | std::ranges::views::transform([](const auto& node) { return static_cast<PassNode*>(node.get()); })  //
                                   | std::ranges::views::filter([](const auto* pass_node) { return !pass_node->m_Cullable; })            //
                                   | std::ranges::to<std::pmr::vector<PassNode*>>();

    const auto present_enabled =
        m_PresentPassNode != nullptr &&
        m_PresentPassNode->swap_chain->GetWidth() != 0 &&
        m_PresentPassNode->swap_chain->GetHeight() != 0;

    if (m_PresentPassNode == nullptr && side_effect_roots.empty()) {
        m_Logger->trace("RenderGraph has no present pass or side-effect root, so nothing will be rendered");
        m_Compiled = true;
        return true;
    }

    if (m_PresentPassNode != nullptr && !present_enabled && side_effect_roots.empty()) {
        m_Logger->trace("The window is minimized, so nothing will be rendered");
        m_Compiled = true;
        return true;
    }

    std::pmr::unordered_set<RenderGraphNode*> essential_nodes;
    {
        const std::function<void(RenderGraphNode*)> do_dfs = [&](RenderGraphNode* node) {
            if (essential_nodes.contains(node)) return;
            essential_nodes.emplace(node);

            for (auto input_node : node->m_InputNodes) {
                do_dfs(input_node);
            }
        };

        // Present is not the only legal side effect. Debug extraction passes write
        // caller-owned resources, so they must seed culling just like present does.
        if (present_enabled) {
            do_dfs(m_PresentPassNode.get());
        }
        for (auto* pass_node : side_effect_roots) {
            do_dfs(pass_node);
        }

        // we need keep all output resource node in essential pass node to avoid execution failure
        auto write_resource_nodes = essential_nodes                                                                    //
                                    | std::ranges::views::filter([](const auto& node) { return node->IsPassNode(); })  //
                                    | std::ranges::views::transform([](auto node) { return node->m_OutputNodes; })     //
                                    | std::ranges::views::join                                                         //
                                    | std::ranges::to<std::pmr::unordered_set<RenderGraphNode*>>();
        essential_nodes.merge(write_resource_nodes);
    }

    {
        const auto sort_by_handle = [this](std::pmr::vector<RenderGraphNode*>& nodes) {
            std::ranges::sort(nodes, [](const auto* lhs, const auto* rhs) {
                return lhs->m_Handle < rhs->m_Handle;
            });
        };

        auto in_degrees = essential_nodes  //
                          | std::ranges::views::transform([&essential_nodes](const auto& node) {
                                return std::make_pair(
                                    node,
                                    std::ranges::count_if(node->m_InputNodes, [&](auto* input_node) {
                                        return essential_nodes.contains(input_node);
                                    }));
                            })  //
                          | std::ranges::to<std::pmr::unordered_map<RenderGraphNode*, std::size_t>>();

        // use vector is ok
        auto start_nodes = in_degrees                                                                       //
                           | std::ranges::views::filter([](const auto& item) { return item.second == 0; })  //
                           | std::ranges::views::keys                                                       //
                           | std::ranges::to<std::pmr::vector<RenderGraphNode*>>();
        sort_by_handle(start_nodes);

        std::size_t num_visited_nodes = 0;
        while (!start_nodes.empty()) {
            ExecuteLayer current_layer;

            std::ranges::for_each(
                start_nodes                                                                                  //
                    | std::ranges::views::filter([](const auto& node) { return node->IsPassNode(); })        //
                    | std::ranges::views::transform([](auto node) { return static_cast<PassNode*>(node); })  //
                ,
                [&](auto node) { current_layer[node->GetCommandType()].emplace_back(node); });
            for (auto& pass_nodes : current_layer) {
                std::ranges::sort(pass_nodes, [](const auto* lhs, const auto* rhs) {
                    return lhs->m_Handle < rhs->m_Handle;
                });
            }

            m_ExecuteLayers.emplace_back(std::move(current_layer));

            std::pmr::vector<RenderGraphNode*> new_start_nodes;
            for (auto node : start_nodes) {
                num_visited_nodes++;
                for (auto output_node : node->m_OutputNodes) {
                    auto it = in_degrees.find(output_node);
                    if (it != in_degrees.end() && --it->second == 0) {
                        new_start_nodes.emplace_back(output_node);
                    }
                }
            }
            sort_by_handle(new_start_nodes);
            start_nodes = std::move(new_start_nodes);
        }

        if (num_visited_nodes != essential_nodes.size()) {
            m_Logger->error("RenderGraph has cycle");
            const auto message = std::format("{} has cycle", m_Name);
            TracyMessageCS(message.data(), message.size(), tracy::Color::Red3, 8);
            m_ExecuteLayers.clear();
            return false;
        }
    }

    for (auto node : essential_nodes) {
        node->Initialize();
    }

    m_Compiled = true;

    return true;
}

auto RenderGraph::Execute() -> std::uint64_t {
    ZoneScoped;

    if (!m_Compiled) {
        m_Logger->warn("RenderGraph has not been compiled");
        TracyMessageLCS("RenderGraph execute skipped because it is not compiled", tracy::Color::OrangeRed3, 8);
        return m_FrameIndex;
    }

    const auto has_executable_passes = std::ranges::any_of(m_ExecuteLayers, [](const auto& execute_layer) {
        return std::ranges::any_of(execute_layer, [](const auto& pass_nodes) {
            return !pass_nodes.empty();
        });
    });

    m_LastLayerProfiles.clear();
    m_LastLayerProfiles.reserve(m_ExecuteLayers.size());

    for (std::size_t layer_index = 0; layer_index < m_ExecuteLayers.size(); ++layer_index) {
        const auto& execute_layer = m_ExecuteLayers[layer_index];
        auto&       layer_profile = m_LastLayerProfiles.emplace_back();

        std::pmr::vector<PassNode*> layer_pass_nodes;
        for (const auto& pass_nodes : execute_layer) {
            layer_pass_nodes.insert(layer_pass_nodes.end(), pass_nodes.begin(), pass_nodes.end());
        }

        const auto record_start = std::chrono::steady_clock::now();
        if (!layer_pass_nodes.empty()) {
            ZoneScopedN("RenderGraph Record Layer");
            const auto layer_name = std::format("Layer {}", layer_index);
            ZoneText(layer_name.data(), layer_name.size());

            for (auto* pass_node : layer_pass_nodes) {
                pass_node->PrepareResourceBarriers();
            }

            for (auto* pass_node : layer_pass_nodes) {
                pass_node->Execute();
            }
        }
        layer_profile.record_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - record_start).count();

        const auto submit_start = std::chrono::steady_clock::now();
        {
            ZoneScopedN("RenderGraph Submit Layer");
            const auto layer_name = std::format("Layer {}", layer_index);
            ZoneText(layer_name.data(), layer_name.size());

            // Improve more fine-grained fence
            magic_enum::enum_for_each<gfx::CommandType>([&](gfx::CommandType type) {
                const auto& pass_nodes = execute_layer[type];

                if (pass_nodes.empty()) return;

                auto commands = pass_nodes  //
                                | std::ranges::views::transform([&](const auto& pass_node) {
                                      return std::cref(*pass_node->m_CommandContext);
                                  })  //
                                | std::ranges::to<std::pmr::vector<std::reference_wrapper<const gfx::CommandContext>>>();

                std::pmr::vector<gfx::FenceWaitInfo> wait_fences;
                magic_enum::enum_for_each<gfx::CommandType>([&](gfx::CommandType wait_type) {
                    if (wait_type == type) return;
                    const auto& fence_value = m_Fences[wait_type];
                    if (fence_value.last_value == 0) return;
                    wait_fences.emplace_back(*fence_value.fence, fence_value.last_value);
                });

                m_Queues.Get(type).Submit(
                    commands,
                    wait_fences,
                    {{gfx::FenceSignalInfo{
                        .fence = *m_Fences[type].fence,
                        .value = ++m_Fences[type].last_value,
                    }}});

                // retire resource
                for (auto pass_node : pass_nodes) {
                    RetireNodesFromPassNode(pass_node, m_Fences[type]);
                }
            });
        }
        layer_profile.submit_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - submit_start).count();
    }

    magic_enum::enum_for_each<gfx::CommandType>([&](gfx::CommandType type) {
        const auto& fence_value = m_Fences[type];
        if (fence_value.last_value != 0) {
            fence_value.fence->Wait(fence_value.last_value);
        }
    });

    RetireNodes();
    Profile();
    Reset();
    return m_FrameIndex++;
}

void RenderGraph::ClearImportedResources() noexcept {
    for (const auto& [resource, handle] : m_ImportedResources) {
        if (!IsValid(gfx_resource_type_to_node_type(resource->GetType()), handle)) continue;

        auto& node = m_Nodes[handle];
        node->m_InputNodes.clear();
        node->m_OutputNodes.clear();
        node.reset();
        m_FreeNodeSlots.emplace_back(handle);
    }

    m_ImportedResources.clear();
    RebuildBlackBoard();
}

void RenderGraph::Reset() noexcept {
    m_Compiled        = false;
    m_PresentPassNode = nullptr;
    m_ExtractionIndex = 0;

    for (std::size_t handle = 0; handle < m_Nodes.size(); handle++) {
        auto& node = m_Nodes[handle];
        if (!node) continue;

        node->m_InputNodes.clear();
        node->m_OutputNodes.clear();

        if (node->IsResourceNode() && static_cast<ResourceNode*>(node.get())->m_IsImported) continue;

        node.reset();
        m_FreeNodeSlots.emplace_back(handle);
    }

    RebuildBlackBoard();
    m_ExecuteLayers.clear();
}

auto RenderGraph::AllocateNode(std::shared_ptr<RenderGraphNode> node) noexcept -> std::size_t {
    if (node == nullptr) return std::numeric_limits<std::size_t>::max();

    std::size_t handle = std::numeric_limits<std::size_t>::max();
    if (!m_FreeNodeSlots.empty()) {
        handle = m_FreeNodeSlots.back();
        m_FreeNodeSlots.pop_back();
        m_Nodes[handle] = std::move(node);
    } else {
        handle = m_Nodes.size();
        m_Nodes.emplace_back(std::move(node));
    }

    m_Nodes[handle]->m_Handle = handle;
    return handle;
}

void RenderGraph::RebuildBlackBoard() noexcept {
    for (auto& black_board : m_BlackBoard) {
        black_board.clear();
    }

    for (const auto& node : m_Nodes) {
        if (!node || !node->IsResourceNode()) continue;

        const auto* resource_node = static_cast<ResourceNode*>(node.get());
        if (!resource_node->m_IsImported || node->GetName().empty()) continue;

        m_BlackBoard[node->GetType()].insert_or_assign(std::pmr::string(node->GetName()), node->m_Handle);
    }
}

void RenderGraph::RetireNodesFromPassNode(PassNode* pass_node, const FenceValue& fence_value) noexcept {
    for (auto input_node : pass_node->m_InputNodes) {
        m_RetiredNodes.emplace_back(RetiredNode{
            .node             = m_Nodes[input_node->m_Handle],
            .last_fence_value = fence_value,
        });
    }
    for (auto output_node : pass_node->m_OutputNodes) {
        m_RetiredNodes.emplace_back(RetiredNode{
            .node             = m_Nodes[output_node->m_Handle],
            .last_fence_value = fence_value,
        });
    }

    m_RetiredNodes.emplace_back(RetiredNode{
        .node             = m_Nodes[pass_node->m_Handle],
        .last_fence_value = fence_value,
    });
}

void RenderGraph::RetireNodes() noexcept {
    const auto latest_fence_values = m_Fences  //
                                     | std::ranges::views::transform([](const auto& fence_value) {
                                           return std::make_pair(fence_value.fence, fence_value.fence->GetCurrentValue());
                                       })  //
                                     | std::ranges::to<std::pmr::unordered_map<std::shared_ptr<gfx::Fence>, std::uint64_t>>();

    while (!m_RetiredNodes.empty()) {
        const auto& retired_resource    = m_RetiredNodes.front();
        const auto& [fence, last_value] = retired_resource.last_fence_value;
        if (latest_fence_values.at(fence) >= last_value) {
            RecycleTransientResource(retired_resource.node.get());
            m_RetiredNodes.pop_front();
        } else {
            break;
        }
    }

    EvictStalePoolEntries();
}

static auto buffer_pool_key(const gfx::GPUBufferDesc& desc) -> std::size_t {
    return utils::combine_hash(std::array{
        utils::hash(desc.size),
        utils::hash(desc.usages),
    });
}

static bool buffer_pool_match(const gfx::GPUBufferDesc& a, const gfx::GPUBufferDesc& b) {
    return a.size == b.size &&
           a.usages == b.usages;
}

static auto texture_pool_key(const gfx::TextureDesc& desc) -> std::size_t {
    return utils::combine_hash(std::array{
        utils::hash(desc.width),
        utils::hash(desc.height),
        utils::hash(static_cast<std::uint16_t>(desc.depth)),
        utils::hash(static_cast<std::uint16_t>(desc.array_size)),
        utils::hash(desc.format),
        utils::hash(static_cast<std::uint16_t>(desc.mip_levels)),
        utils::hash(desc.usages),
    });
}

static bool texture_pool_match(const gfx::TextureDesc& a, const gfx::TextureDesc& b) {
    return a.width == b.width &&
           a.height == b.height &&
           a.depth == b.depth &&
           a.array_size == b.array_size &&
           a.format == b.format &&
           a.mip_levels == b.mip_levels &&
           a.clear_value.has_value() == b.clear_value.has_value() &&
           a.usages == b.usages;
}

auto RenderGraph::AcquireTransientBuffer(const gfx::GPUBufferDesc& desc) -> std::shared_ptr<gfx::GPUBuffer> {
    const auto key   = buffer_pool_key(desc);
    auto       range = m_TransientPool.buffers.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
        if (buffer_pool_match(it->second.desc, desc)) {
            auto resource = std::move(it->second.resource);
            m_TransientPool.buffer_bytes -= it->second.byte_size;
            m_TransientPool.buffers.erase(it);
            m_Logger->trace("Reused transient buffer from pool: {}", desc.name);
            return resource;
        }
    }
    return hitagi::gfx::GPUBuffer::Create(m_Device, desc);
}

auto RenderGraph::AcquireTransientTexture(const gfx::TextureDesc& desc) -> std::shared_ptr<gfx::Texture> {
    const auto key   = texture_pool_key(desc);
    auto       range = m_TransientPool.textures.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
        if (texture_pool_match(it->second.desc, desc)) {
            auto resource = std::move(it->second.resource);
            m_TransientPool.texture_bytes -= it->second.byte_size;
            m_TransientPool.textures.erase(it);
            m_Logger->trace("Reused transient texture from pool: {}", desc.name);
            return resource;
        }
    }
    return hitagi::gfx::Texture::Create(m_Device, m_Queues, m_Bindings, desc);
}

void RenderGraph::RecycleTransientResource(RenderGraphNode* node) noexcept {
    if (!node->IsResourceNode()) return;

    auto* resource_node = static_cast<ResourceNode*>(node);
    if (resource_node->m_IsImported || !resource_node->m_Resource) return;

    switch (node->GetType()) {
        case RenderGraphNode::Type::GPUBuffer: {
            auto* buffer_node = static_cast<GPUBufferNode*>(node);
            if (buffer_node->m_MoveFromNode || buffer_node->m_MoveToNode) return;
            auto       resource  = std::static_pointer_cast<gfx::GPUBuffer>(resource_node->m_Resource);
            const auto byte_size = resource->GetAllocationSize();
            m_TransientPool.buffers.emplace(
                buffer_pool_key(buffer_node->GetDesc()),
                TransientResourcePool::CachedBuffer{
                    .desc            = buffer_node->GetDesc(),
                    .resource        = std::move(resource),
                    .last_used_frame = m_FrameIndex,
                    .byte_size       = byte_size,
                });
            m_TransientPool.buffer_bytes += byte_size;
            resource_node->m_Resource = nullptr;
        } break;
        case RenderGraphNode::Type::Texture: {
            auto* texture_node = static_cast<TextureNode*>(node);
            if (texture_node->m_MoveFromNode || texture_node->m_MoveToNode) return;
            auto       resource  = std::static_pointer_cast<gfx::Texture>(resource_node->m_Resource);
            const auto byte_size = resource->GetAllocationSize();
            m_TransientPool.textures.emplace(
                texture_pool_key(texture_node->GetDesc()),
                TransientResourcePool::CachedTexture{
                    .desc            = texture_node->GetDesc(),
                    .resource        = std::move(resource),
                    .last_used_frame = m_FrameIndex,
                    .byte_size       = byte_size,
                });
            m_TransientPool.texture_bytes += byte_size;
            resource_node->m_Resource = nullptr;
        } break;
        default:
            break;
    }
}

void RenderGraph::EvictStalePoolEntries() noexcept {
    for (auto it = m_TransientPool.buffers.begin(); it != m_TransientPool.buffers.end();) {
        if (m_FrameIndex - it->second.last_used_frame > TransientResourcePool::max_unused_frames) {
            m_TransientPool.buffer_bytes -= it->second.byte_size;
            it = m_TransientPool.buffers.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_TransientPool.textures.begin(); it != m_TransientPool.textures.end();) {
        if (m_FrameIndex - it->second.last_used_frame > TransientResourcePool::max_unused_frames ||
            m_TransientPool.texture_bytes > TransientResourcePool::max_texture_pool_bytes) {
            m_TransientPool.texture_bytes -= it->second.byte_size;
            it = m_TransientPool.textures.erase(it);
        } else {
            ++it;
        }
    }
}

auto RenderGraph::GetTransientPoolStats() const noexcept -> TransientPoolStats {
    return {
        .buffer_bytes  = m_TransientPool.buffer_bytes,
        .texture_bytes = m_TransientPool.texture_bytes,
        .buffer_count  = m_TransientPool.buffers.size(),
        .texture_count = m_TransientPool.textures.size(),
    };
}

auto RenderGraph::ToDot() const noexcept -> std::pmr::string {
    const auto node_writer = [&](const RenderGraphNode* node) {
        std::string_view shape = node->IsResourceNode() ? "shape=box " : "";
        return std::pmr::string(std::format(R"({}label="{}\nhandle: {}")", shape, node->GetName(), node->m_Handle));
    };

    const auto edge_writer = [&](const RenderGraphNode* from, const RenderGraphNode* to) {
        const PassNode*     pass_node     = nullptr;
        const ResourceNode* resource_node = nullptr;
        if (from->IsPassNode()) {
            pass_node     = static_cast<const PassNode*>(from);
            resource_node = static_cast<const ResourceNode*>(to);
        } else if (to->IsPassNode()) {
            pass_node     = static_cast<const PassNode*>(to);
            resource_node = static_cast<const ResourceNode*>(from);
        }

        if (pass_node) {
            std::pmr::string edge;
            for (const auto& access : pass_node->m_GPUBufferEdges) {
                if (access.resource != resource_node) continue;
                if (!edge.empty()) edge += "\\n";
                edge += std::format("{},{}; offset={}, count={}, stride={}",
                                    magic_enum::enum_name(access.access), magic_enum::enum_name(access.stage),
                                    access.view_desc.offset, access.view_desc.element_count, access.view_desc.element_stride);
            }
            for (const auto& access : pass_node->m_TextureEdges) {
                if (access.resource != resource_node) continue;
                if (!edge.empty()) edge += "\\n";
                edge += std::format("{},{},{}; mip={}, layers={}",
                                    magic_enum::enum_name(access.access), magic_enum::enum_name(access.layout), magic_enum::enum_name(access.stage),
                                    access.view_desc.base_mip_level, access.view_desc.layer_count);
            }
            return std::pmr::string(std::format("label=\"{}\"", edge));
        }
        // move edge
        else {
            return std::pmr::string("style=dashed");
        }
    };

    std::pmr::string output = "digraph {\n";

    for (const auto& node : m_Nodes) {
        if (!node) continue;
        output += std::format("  {} [{}];\n", node->m_Handle, node_writer(node.get()));
    }

    for (const auto& from_node : m_Nodes) {
        if (!from_node) continue;
        for (auto to_node : from_node->m_OutputNodes) {
            if (!to_node) continue;
            output += std::format(
                "  {} -> {} [{}];\n",
                from_node->m_Handle, to_node->m_Handle,
                edge_writer(from_node.get(), to_node));
        }
    }

    output += "}\n";
    return output;
}

void RenderGraph::Profile() const noexcept {
    static bool configured = false;
    if (!configured) {
        TracyPlotConfig("Retired Resource Counts", tracy::PlotFormatType::Number, true, true, 0);
        TracyPlotConfig("Transient Buffer Count", tracy::PlotFormatType::Number, true, true, 0);
        TracyPlotConfig("Transient Texture Count", tracy::PlotFormatType::Number, true, true, 0);
        TracyPlotConfig("RenderGraph Execute Layers", tracy::PlotFormatType::Number, true, true, 0);
        TracyPlotConfig("RenderGraph Record Time (ms)", tracy::PlotFormatType::Number, false, true, 0);
        TracyPlotConfig("RenderGraph Submit Time (ms)", tracy::PlotFormatType::Number, false, true, 0);
        TracyPlotConfig("RenderGraph Max Layer Record Time (ms)", tracy::PlotFormatType::Number, false, true, 0);
        TracyPlotConfig("RenderGraph Max Layer Submit Time (ms)", tracy::PlotFormatType::Number, false, true, 0);
        configured = true;
    }
    double total_record_ms = 0.0;
    double total_submit_ms = 0.0;
    double max_record_ms   = 0.0;
    double max_submit_ms   = 0.0;
    for (const auto& profile : m_LastLayerProfiles) {
        total_record_ms += profile.record_ms;
        total_submit_ms += profile.submit_ms;
        max_record_ms = std::max(max_record_ms, profile.record_ms);
        max_submit_ms = std::max(max_submit_ms, profile.submit_ms);
    }

    TracyPlot("Retired Resource Counts", static_cast<std::int64_t>(m_RetiredNodes.size()));
    TracyPlot("Transient Buffer Count", static_cast<std::int64_t>(m_TransientPool.buffers.size()));
    TracyPlot("Transient Texture Count", static_cast<std::int64_t>(m_TransientPool.textures.size()));
    TracyPlot("RenderGraph Execute Layers", static_cast<std::int64_t>(m_ExecuteLayers.size()));
    TracyPlot("RenderGraph Record Time (ms)", total_record_ms);
    TracyPlot("RenderGraph Submit Time (ms)", total_submit_ms);
    TracyPlot("RenderGraph Max Layer Record Time (ms)", max_record_ms);
    TracyPlot("RenderGraph Max Layer Submit Time (ms)", max_submit_ms);
}

}  // namespace hitagi::rg
