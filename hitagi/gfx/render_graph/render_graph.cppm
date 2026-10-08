module;
#include <spdlog/logger.h>

export module gfx.render_graph;
import std;
import utils;
import gfx.base;

export namespace hitagi::rg {

class RenderGraph;
class PassBuilder;

class RenderGraphNode;

class ResourceNode;
class GPUBufferNode;
class TextureNode;
class SamplerNode;

class PassNode;
class RenderPassNode;
class ComputePassNode;
class CopyPassNode;
class PresentPassNode;

class RenderGraphEdge;

class RenderGraphNode {
public:
    friend RenderGraph;
    friend PassBuilder;

    enum struct Type : std::uint8_t {
        GPUBuffer,
        Texture,
        Sampler,
        RenderPass,
        ComputePass,
        CopyPass,
        PresentPass,
    };

    RenderGraphNode(const RenderGraphNode&)            = delete;
    RenderGraphNode(RenderGraphNode&&) noexcept        = default;
    RenderGraphNode& operator=(const RenderGraphNode&) = delete;
    RenderGraphNode& operator=(RenderGraphNode&&)      = default;
    virtual ~RenderGraphNode()                         = default;

    inline auto GetName() const noexcept -> std::string_view { return m_Name; }
    inline auto GetType() const noexcept { return m_Type; }

    inline bool IsResourceNode() const noexcept {
        return m_Type == Type::GPUBuffer ||
               m_Type == Type::Texture ||
               m_Type == Type::Sampler;
    }
    inline bool IsPassNode() const noexcept { return m_Type == Type::RenderPass || m_Type == Type::ComputePass || m_Type == Type::CopyPass || m_Type == Type::PresentPass; }

    void AddInputNode(RenderGraphNode* node) noexcept;

protected:
    RenderGraphNode(RenderGraph& render_graph, Type type, std::string_view name = "")
        : m_RenderGraph(&render_graph), m_Type(type), m_Name(name) {}

    virtual void Initialize() = 0;

    RenderGraph* m_RenderGraph;
    Type         m_Type;

    std::pmr::unordered_set<RenderGraphNode*> m_InputNodes;
    std::pmr::unordered_set<RenderGraphNode*> m_OutputNodes;

    std::size_t      m_Handle;
    std::pmr::string m_Name;
};

inline constexpr auto gfx_resource_type_to_node_type(gfx::ResourceType type) {
    switch (type) {
        case gfx::ResourceType::GPUBuffer:
            return RenderGraphNode::Type::GPUBuffer;
        case gfx::ResourceType::Texture:
            return RenderGraphNode::Type::Texture;
        case gfx::ResourceType::Sampler:
            return RenderGraphNode::Type::Sampler;
        default:
            throw std::invalid_argument("Invalid resource type");
    }
}

struct GPUBufferEdge;
struct TextureEdge;

template <RenderGraphNode::Type T>
struct RenderGraphHandle {
    constexpr RenderGraphHandle(std::size_t id = std::numeric_limits<std::size_t>::max()) noexcept : index(id) {}
    inline constexpr std::strong_ordering operator<=>(const RenderGraphHandle&) const noexcept = default;
    inline constexpr bool                 operator==(const RenderGraphHandle&) const noexcept  = default;
    inline explicit constexpr             operator bool() const noexcept { return index != std::numeric_limits<std::size_t>::max(); }

    std::size_t           index;
    RenderGraphNode::Type type = T;
};

using GPUBufferHandle   = RenderGraphHandle<RenderGraphNode::Type::GPUBuffer>;
using TextureHandle     = RenderGraphHandle<RenderGraphNode::Type::Texture>;
using SamplerHandle     = RenderGraphHandle<RenderGraphNode::Type::Sampler>;
using RenderPassHandle  = RenderGraphHandle<RenderGraphNode::Type::RenderPass>;
using ComputePassHandle = RenderGraphHandle<RenderGraphNode::Type::ComputePass>;
using CopyPassHandle    = RenderGraphHandle<RenderGraphNode::Type::CopyPass>;
using PresentPassHandle = RenderGraphHandle<RenderGraphNode::Type::PresentPass>;

}  // namespace hitagi::rg

export namespace std {
template <hitagi::rg::RenderGraphNode::Type T>
struct hash<hitagi::rg::RenderGraphHandle<T>> {
    constexpr inline size_t operator()(const hitagi::rg::RenderGraphHandle<T>& handle) const noexcept {
        return hash<std::size_t>()(handle.index);
    }
};
}  // namespace std

export namespace hitagi::rg {

// Access handles belong to one pass instance, not to a resource or a view node.
template <RenderGraphNode::Type T>
struct ResourceEdgeHandle {
    std::uint64_t owner = 0;
    std::size_t   index = std::numeric_limits<std::size_t>::max();
    explicit      operator bool() const noexcept { return owner != 0; }
    bool          operator==(const ResourceEdgeHandle&) const noexcept = default;
};
using GPUBufferEdgeHandle = ResourceEdgeHandle<RenderGraphNode::Type::GPUBuffer>;
using TextureEdgeHandle   = ResourceEdgeHandle<RenderGraphNode::Type::Texture>;

struct GPUBufferEdge {
    GPUBufferNode*                      resource = nullptr;
    bool                                write    = false;
    gfx::BarrierAccess                  access{};
    gfx::PipelineStage                  stage{};
    gfx::GPUBufferViewDesc              view_desc;
    bool                                create_view = true;
    std::shared_ptr<gfx::GPUBufferView> view;
};

struct TextureEdge {
    TextureNode*                      resource = nullptr;
    bool                              write    = false;
    gfx::BarrierAccess                access{};
    gfx::PipelineStage                stage{};
    gfx::TextureLayout                layout{};
    gfx::TextureViewDesc              view_desc;
    bool                              create_view = true;
    std::shared_ptr<gfx::TextureView> view;
};

class ResourceNode : public RenderGraphNode {
public:
    friend RenderGraph;

    auto GetWriter() const noexcept -> PassNode*;

protected:
    ResourceNode(RenderGraph& render_graph, Type type, std::string_view name = "", std::shared_ptr<gfx::Resource> resource = nullptr);

    bool                           m_IsImported = false;
    std::shared_ptr<gfx::Resource> m_Resource;
};

class GPUBufferNode : public ResourceNode {
public:
    friend RenderGraph;

    GPUBufferNode(RenderGraph& render_graph, gfx::GPUBufferDesc desc, std::string_view name = "");
    GPUBufferNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> buffer, std::string_view name = "");

    inline auto& Resolve() const noexcept { return static_cast<gfx::GPUBuffer&>(*m_Resource); }
    inline auto  GetBuffer() const noexcept -> std::shared_ptr<gfx::GPUBuffer> { return std::static_pointer_cast<gfx::GPUBuffer>(m_Resource); }
    auto         GetDesc() const noexcept -> const gfx::GPUBufferDesc&;

    auto        Move(GPUBufferHandle new_handle, std::string_view new_name) -> std::shared_ptr<GPUBufferNode>;
    inline auto GetMoveNode() const noexcept { return m_MoveToNode; }
    inline auto GetMoveFromNode() const noexcept { return m_MoveFromNode; }

protected:
    void Initialize() final;

    std::optional<gfx::GPUBufferDesc> m_Desc;
    GPUBufferNode*                    m_MoveToNode   = nullptr;
    GPUBufferNode*                    m_MoveFromNode = nullptr;
};

class TextureNode : public ResourceNode {
public:
    friend RenderGraph;

    TextureNode(RenderGraph& render_graph, gfx::TextureDesc desc, std::string_view name = "");
    TextureNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> texture, std::string_view name = "");

    inline auto& Resolve() const noexcept { return static_cast<gfx::Texture&>(*m_Resource); }
    inline auto  GetTexture() const noexcept -> std::shared_ptr<gfx::Texture> { return std::static_pointer_cast<gfx::Texture>(m_Resource); }
    auto         GetDesc() const noexcept -> const gfx::TextureDesc&;

    auto        Move(TextureHandle new_handle, std::string_view new_name) -> std::shared_ptr<TextureNode>;
    inline auto GetMoveNode() const noexcept { return m_MoveToNode; }
    inline auto GetMoveFromNode() const noexcept { return m_MoveFromNode; }

protected:
    void Initialize() final;

    std::optional<gfx::TextureDesc> m_Desc;
    TextureNode*                    m_MoveToNode   = nullptr;
    TextureNode*                    m_MoveFromNode = nullptr;
};

class SamplerNode : public ResourceNode {
public:
    friend RenderGraph;

    SamplerNode(RenderGraph& render_graph, gfx::SamplerDesc desc, std::string_view name = "");
    SamplerNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> sampler, std::string_view name = "");

    inline auto& Resolve() const noexcept { return static_cast<gfx::Sampler&>(*m_Resource); }
    auto         GetDesc() const noexcept -> const gfx::SamplerDesc&;

protected:
    void Initialize() final;

    std::optional<gfx::SamplerDesc> m_Desc;
};

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
    void SetPassCullingAllowed(bool allow) noexcept;
    auto AddGPUBufferEdge(GPUBufferHandle buffer_handle, GPUBufferEdge edge) noexcept -> GPUBufferEdgeHandle;
    auto AddTextureEdge(TextureHandle texture_handle, TextureEdge edge) noexcept -> TextureEdgeHandle;
    void AddSamplerEdge(SamplerHandle sampler_handle) noexcept;

    auto Finish() -> std::size_t;

    RenderGraph& m_RenderGraph;
    bool         m_Invalid  = false;
    bool         m_Finished = false;

    std::shared_ptr<PassNode> pass_base;
};

class RenderPassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    RenderPassBuilder(RenderGraph& render_graph);

    void SetName(std::string_view name) noexcept;
    void AllowPassCulling(bool allow) noexcept;

    auto Read(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept -> GPUBufferEdgeHandle;
    auto Read(TextureHandle texture, gfx::TextureViewDesc desc = {}, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept -> TextureEdgeHandle;
    auto ReadAsVertices(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}) noexcept -> GPUBufferEdgeHandle;
    auto ReadAsIndices(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}) noexcept -> GPUBufferEdgeHandle;

    auto Write(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept -> GPUBufferEdgeHandle;
    auto Write(TextureHandle texture, gfx::TextureViewDesc desc = {}, gfx::PipelineStage stage = gfx::PipelineStage::All) noexcept -> TextureEdgeHandle;

    auto SetRenderTarget(TextureHandle texture, bool clear = false, gfx::TextureViewDesc desc = {.mip_levels = 1, .layer_count = 1}) noexcept -> TextureEdgeHandle;
    auto SetDepthStencil(TextureHandle texture, bool clear = false, gfx::TextureViewDesc desc = {.mip_levels = 1, .layer_count = 1}) noexcept -> TextureEdgeHandle;
    auto ReadDepthStencil(TextureHandle texture, gfx::TextureViewDesc desc = {.mip_levels = 1, .layer_count = 1}) noexcept -> TextureEdgeHandle;

    void AddSampler(SamplerHandle sampler) noexcept;

    void SetExecutor(RenderPassNode::Executor executor) noexcept;

    auto Finish() noexcept -> RenderPassHandle;

private:
    std::shared_ptr<RenderPassNode> pass;
};

class ComputePassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    ComputePassBuilder(RenderGraph& render_graph);

    void SetName(std::string_view name) noexcept;
    void AllowPassCulling(bool allow) noexcept;

    auto Read(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}) noexcept -> GPUBufferEdgeHandle;
    auto Read(TextureHandle texture, gfx::TextureViewDesc desc = {}) noexcept -> TextureEdgeHandle;

    auto Write(GPUBufferHandle buffer, gfx::GPUBufferViewDesc desc = {.element_count = 0}) noexcept -> GPUBufferEdgeHandle;
    auto Write(TextureHandle texture, gfx::TextureViewDesc desc = {}) noexcept -> TextureEdgeHandle;

    void AddSampler(SamplerHandle sampler) noexcept;

    void SetExecutor(ComputePassNode::Executor executor) noexcept;

    auto Finish() noexcept -> ComputePassHandle;

private:
    std::shared_ptr<ComputePassNode> pass;
};

class CopyPassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    CopyPassBuilder(RenderGraph& render_graph);

    void SetName(std::string_view name) noexcept;
    void AllowPassCulling(bool allow) noexcept;

    void BufferToBuffer(GPUBufferHandle src, GPUBufferHandle dst) noexcept;
    void BufferToTexture(GPUBufferHandle src, TextureHandle dst, gfx::TextureSubresourceLayer layer = {}) noexcept;
    void TextureToBuffer(TextureHandle src, GPUBufferHandle dst, gfx::TextureSubresourceLayer layer = {}) noexcept;
    void TextureToTexture(TextureHandle src, TextureHandle dst, gfx::TextureSubresourceLayer src_layer = {}, gfx::TextureSubresourceLayer dst_layer = {}) noexcept;

    void SetExecutor(CopyPassNode::Executor executor) noexcept;

    auto Finish() noexcept -> CopyPassHandle;

private:
    std::shared_ptr<CopyPassNode> pass;
};

class PresentPassBuilder : public PassBuilder {
public:
    friend RenderGraph;

    PresentPassBuilder(RenderGraph& render_graph);

    void From(TextureHandle texture, gfx::TextureSubresourceLayer layer = {}) noexcept;
    void SetSwapChain(const std::shared_ptr<gfx::SwapChain>& swap_chain) noexcept;

    void Finish() noexcept;

private:
    std::shared_ptr<PresentPassNode> pass;
};

class RenderGraph {
public:
    friend class DependencyGraph;

    RenderGraph(gfx::Device& device, std::string_view name = "RenderGraph");
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

    gfx::Device& m_Device;

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

template <RenderGraphNode::Type T>
bool RenderGraph::IsValid(RenderGraphHandle<T> handle) const noexcept {
    return IsValid(T, handle.index);
}

}  // namespace hitagi::rg
