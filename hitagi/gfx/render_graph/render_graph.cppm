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
class RenderPipelineNode;
class ComputePipelineNode;

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
        RenderPipeline,
        ComputePipeline,
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
               m_Type == Type::Sampler ||
               m_Type == Type::RenderPipeline ||
               m_Type == Type::ComputePipeline;
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
        case gfx::ResourceType::RenderPipeline:
            return RenderGraphNode::Type::RenderPipeline;
        case gfx::ResourceType::ComputePipeline:
            return RenderGraphNode::Type::ComputePipeline;
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

using GPUBufferHandle       = RenderGraphHandle<RenderGraphNode::Type::GPUBuffer>;
using TextureHandle         = RenderGraphHandle<RenderGraphNode::Type::Texture>;
using SamplerHandle         = RenderGraphHandle<RenderGraphNode::Type::Sampler>;
using RenderPipelineHandle  = RenderGraphHandle<RenderGraphNode::Type::RenderPipeline>;
using ComputePipelineHandle = RenderGraphHandle<RenderGraphNode::Type::ComputePipeline>;
using RenderPassHandle      = RenderGraphHandle<RenderGraphNode::Type::RenderPass>;
using ComputePassHandle     = RenderGraphHandle<RenderGraphNode::Type::ComputePass>;
using CopyPassHandle        = RenderGraphHandle<RenderGraphNode::Type::CopyPass>;
using PresentPassHandle     = RenderGraphHandle<RenderGraphNode::Type::PresentPass>;

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

struct GPUBufferEdge {
    bool               write;
    gfx::BarrierAccess access;
    gfx::PipelineStage stage;
    std::size_t        element_offset;
    std::size_t        num_elements;

    std::pmr::vector<gfx::BindlessHandle> bindless_handles;

    bool operator==(const GPUBufferEdge& rhs) const noexcept {
        return write == rhs.write &&
               access == rhs.access &&
               stage == rhs.stage &&
               element_offset == rhs.element_offset &&
               num_elements == rhs.num_elements;
    }
};

struct TextureEdge {
    bool                         write;
    gfx::BarrierAccess           access;
    gfx::PipelineStage           stage;
    gfx::TextureLayout           layout;
    gfx::TextureSubresourceLayer layer;

    gfx::BindlessHandle bindless;

    bool operator==(const TextureEdge& rhs) const noexcept {
        return write == rhs.write &&
               access == rhs.access &&
               layout == rhs.layout &&
               stage == rhs.stage;
    }
};

struct SamplerEdge {
    gfx::BindlessHandle bindless;
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

class RenderPipelineNode : public ResourceNode {
public:
    friend RenderGraph;

    RenderPipelineNode(RenderGraph& render_graph, gfx::RenderPipelineDesc desc, std::string_view name = "");
    RenderPipelineNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> pipeline, std::string_view name = "");

    inline auto& Resolve() const noexcept { return static_cast<gfx::RenderPipeline&>(*m_Resource); }
    auto         GetDesc() const noexcept -> const gfx::RenderPipelineDesc&;

protected:
    void Initialize() final;

    std::optional<gfx::RenderPipelineDesc> m_Desc;
};

class ComputePipelineNode : public ResourceNode {
public:
    friend RenderGraph;

    ComputePipelineNode(RenderGraph& render_graph, gfx::ComputePipelineDesc desc, std::string_view name = "");
    ComputePipelineNode(RenderGraph& render_graph, std::shared_ptr<gfx::Resource> pipeline, std::string_view name = "");

    inline auto& Resolve() const noexcept { return static_cast<gfx::ComputePipeline&>(*m_Resource); }
    auto         GetDesc() const noexcept -> const gfx::ComputePipelineDesc&;

protected:
    void Initialize() final;

    std::optional<gfx::ComputePipelineDesc> m_Desc;
};



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

    std::pmr::unordered_map<GPUBufferNode*, GPUBufferEdge> m_GPUBufferEdges;
    std::pmr::unordered_map<TextureNode*, TextureEdge>     m_TextureEdges;
    std::pmr::unordered_map<SamplerNode*, SamplerEdge>     m_SamplerEdges;
    std::pmr::unordered_set<RenderPipelineNode*>           m_RenderPipelines;
    std::pmr::unordered_set<ComputePipelineNode*>          m_ComputePipelines;

    std::pmr::vector<gfx::GPUBufferBarrier> m_GPUBufferBarriers;
    std::pmr::vector<gfx::TextureBarrier>   m_TextureBarriers;

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

    Executor     m_Executor;
    TextureNode* m_RenderTarget      = nullptr;
    TextureNode* m_DepthStencil      = nullptr;
    bool         m_ClearRenderTarget = false;
    bool         m_ClearDepthStencil = false;
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
    void AddGPUBufferEdge(GPUBufferHandle buffer_handle, GPUBufferEdge edge) noexcept;
    void AddTextureEdge(TextureHandle texture_handle, TextureEdge edge) noexcept;
    void AddSamplerEdge(SamplerHandle sampler_handle, SamplerEdge edge) noexcept;

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

    RenderPassBuilder& SetName(std::string_view name) noexcept;
    RenderPassBuilder& AllowPassCulling(bool allow) noexcept;

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
    ComputePassBuilder& AllowPassCulling(bool allow) noexcept;

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
    CopyPassBuilder& AllowPassCulling(bool allow) noexcept;

    CopyPassBuilder& BufferToBuffer(GPUBufferHandle src, GPUBufferHandle dst) noexcept;
    CopyPassBuilder& BufferToTexture(GPUBufferHandle src, TextureHandle dst, gfx::TextureSubresourceLayer layer = {}) noexcept;
    CopyPassBuilder& TextureToBuffer(TextureHandle src, GPUBufferHandle dst, gfx::TextureSubresourceLayer layer = {}) noexcept;
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

    auto QueueTextureExtraction(TextureHandle from, std::shared_ptr<gfx::Texture> to, gfx::TextureSubresourceLayer from_layer = {}, gfx::TextureSubresourceLayer to_layer = {}) noexcept -> CopyPassHandle;
    auto QueueBufferExtraction(TextureHandle from, std::shared_ptr<gfx::GPUBuffer> to, gfx::TextureSubresourceLayer from_layer = {}) noexcept -> CopyPassHandle;

    bool Compile();
    auto Execute() -> std::uint64_t;
    void ClearImportedResources() noexcept;

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
    friend GPUBufferNode;
    friend TextureNode;
    friend PassNode;

    using ResourceDesc = std::variant<gfx::GPUBufferDesc, gfx::TextureDesc, gfx::SamplerDesc, gfx::RenderPipelineDesc, gfx::ComputePipelineDesc>;
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

    struct TransientResourcePool {
        static constexpr std::uint64_t max_unused_frames = 3;

        struct CachedBuffer {
            gfx::GPUBufferDesc              desc;
            std::shared_ptr<gfx::GPUBuffer> resource;
            std::uint64_t                   last_used_frame = 0;
        };
        struct CachedTexture {
            gfx::TextureDesc              desc;
            std::shared_ptr<gfx::Texture> resource;
            std::uint64_t                 last_used_frame = 0;
        };

        std::unordered_multimap<std::size_t, CachedBuffer>  buffers;
        std::unordered_multimap<std::size_t, CachedTexture> textures;
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
